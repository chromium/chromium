// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/extend.h"
#include "base/containers/flat_tree.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "skia/ext/image_operations.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using ReadCompressedIconCallback =
    base::OnceCallback<void(std::vector<uint8_t> data)>;

using ReadIconCallback = base::OnceCallback<void(SkBitmap)>;

constexpr base::FilePath::CharType kIconsAnyDirectoryName[] =
    FILE_PATH_LITERAL("Icons");
constexpr base::FilePath::CharType kIconsMonochromeDirectoryName[] =
    FILE_PATH_LITERAL("Icons Monochrome");
constexpr base::FilePath::CharType kIconsMaskableDirectoryName[] =
    FILE_PATH_LITERAL("Icons Maskable");
constexpr base::FilePath::CharType kTrustedIconFolderName[] =
    FILE_PATH_LITERAL("Trusted Icons");
constexpr base::FilePath::CharType kPendingTrustedIconFolderName[] =
    FILE_PATH_LITERAL("Pending Trusted Icons");
constexpr base::FilePath::CharType kPendingManifestIconFolderName[] =
    FILE_PATH_LITERAL("Pending Manifest Icons");

// Used to identify the folder from which to read the icons from.
enum class ReadConfiguration {
  kManifestIcons = 0,
  kTrustedIcons = 1,
  kPendingTrustedIcons = 2,
  kMaxValue = kPendingTrustedIcons,
};

// Records the result of reading trusted icons from disk to UMA.
void RecordTrustedIconsReadResult(bool trusted_icon_used) {
  base::UmaHistogramBoolean("WebApp.TrustedIcons.ReadResult",
                            trusted_icon_used);
}

// This utility struct is to carry error logs between threads via return values.
// If we weren't generating multithreaded errors we would just append the errors
// to WebAppIconManager::error_log() directly.
template <typename T>
struct TypedResult {
  T value = T();
  std::vector<std::string> error_log;

  bool HasErrors() const { return !error_log.empty(); }
};

std::string CreateError(std::initializer_list<std::string_view> parts,
                        bool skip_logging = false) {
  std::string error = base::StrCat(parts);
  if (!skip_logging) {
    LOG(ERROR) << error;
  }
  return error;
}

// This is not a method on WebAppIconManager to avoid having to expose
// TypedResult<T> beyond this cc file.
template <typename T>
void LogErrorsCallCallback(base::WeakPtr<WebAppIconManager> manager,
                           base::OnceCallback<void(T)> callback,
                           TypedResult<T> result) {
  if (!manager)
    return;
  std::vector<std::string>* error_log = manager->error_log();
  if (error_log) {
    base::Extend(*error_log, std::move(result.error_log));
  }

  std::move(callback).Run(std::move(result.value));
}

struct IconId {
  IconId(webapps::AppId app_id, IconPurpose purpose, SquareSizePx size)
      : app_id(std::move(app_id)), purpose(purpose), size(size) {}
  ~IconId() = default;

  webapps::AppId app_id;
  IconPurpose purpose;
  SquareSizePx size;
};

base::FilePath GetRelativeDirectoryForPurpose(IconPurpose purpose) {
  switch (purpose) {
    case IconPurpose::ANY:
      return base::FilePath(kIconsAnyDirectoryName);
    case IconPurpose::MONOCHROME:
      return base::FilePath(kIconsMonochromeDirectoryName);
    case IconPurpose::MASKABLE:
      return base::FilePath(kIconsMaskableDirectoryName);
  }
}

// This is a private implementation detail of WebAppIconManager, where and how
// to store shortcuts menu icons files.
// All of the other shortcut icon directories appear under the directory for
// |ANY|.
base::FilePath GetAppShortcutsMenuIconsRelativeDirectory(IconPurpose purpose) {
  constexpr base::FilePath::CharType kShortcutsMenuIconsDirectoryName[] =
      FILE_PATH_LITERAL("Shortcuts Menu Icons");

  constexpr base::FilePath::CharType
      kShortcutsMenuIconsMonochromeDirectoryName[] =
          FILE_PATH_LITERAL("Monochrome");
  constexpr base::FilePath::CharType
      kShortcutsMenuIconsMaskableDirectoryName[] =
          FILE_PATH_LITERAL("Maskable");

  base::FilePath shortcuts_icons_directory(kShortcutsMenuIconsDirectoryName);

  switch (purpose) {
    case IconPurpose::ANY:
      return shortcuts_icons_directory;
    case IconPurpose::MONOCHROME:
      return shortcuts_icons_directory.Append(
          kShortcutsMenuIconsMonochromeDirectoryName);
    case IconPurpose::MASKABLE:
      return shortcuts_icons_directory.Append(
          kShortcutsMenuIconsMaskableDirectoryName);
  }
}

base::FilePath GetOtherIconsRelativeDirectory() {
  return base::FilePath(FILE_PATH_LITERAL("Image Cache"));
}

// Returns a string suitable for use as a directory for the given URL. This name
// is a hash of the URL.
std::string GetDirectoryNameForUrl(const GURL& url) {
  return base::NumberToString(base::PersistentHash(url.spec()));
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool DeleteDataBlocking(scoped_refptr<FileUtilsWrapper> utils,
                        const base::FilePath& web_apps_directory,
                        const webapps::AppId& app_id) {
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id);

  return utils->DeleteFileRecursively(app_dir);
}

// `web_apps_directory` is the path to the directory where all web app data is
// stored for the relevant profile. Appends `child_directory` to the top level
// directory where web app icons are stored, and returns the icon files stored
// in that directory corresponding to `icon_id`.
base::FilePath GetIconsFileNameForChildDirectory(
    const base::FilePath& web_apps_directory,
    const base::FilePath& child_directory,
    const IconId& icon_id) {
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, icon_id.app_id);
  return app_dir.Append(child_directory)
      .Append(GetRelativeDirectoryForPurpose(icon_id.purpose))
      .AppendASCII(base::StringPrintf("%i.png", icon_id.size));
}

// `web_apps_directory` is the path to the directory where all web app data is
// stored for the relevant profile.
base::FilePath GetIconFileName(const base::FilePath& web_apps_directory,
                               const IconId& icon_id) {
  base::FilePath app_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, icon_id.app_id);
  base::FilePath icons_dir =
      app_dir.Append(GetRelativeDirectoryForPurpose(icon_id.purpose));

  return icons_dir.AppendASCII(base::StringPrintf("%i.png", icon_id.size));
}

// This function takes a directory path, and a list of file paths that are
// expected in the directory. It will delete any files not in `keep_files`.
// Returns if the operations all succeeded.
TypedResult<bool> DeleteOtherFilesInDir(
    const base::FilePath& directory,
    const absl::flat_hash_set<base::FilePath>& keep_files) {
  TypedResult<bool> result{.value = true};

  if (!base::DirectoryExists(directory)) {
    return result;
  }

  std::vector<base::FilePath> files_to_delete;
  base::FileEnumerator dest_file_enumerator(directory,
                                            /*recursive=*/false,
                                            base::FileEnumerator::FILES);

  dest_file_enumerator.ForEach([&](const base::FilePath& file_path) {
    if (!keep_files.contains(file_path)) {
      files_to_delete.push_back(std::move(file_path));
    }
  });

  for (const base::FilePath& file_path : files_to_delete) {
    if (!base::DeleteFile(file_path)) {
      result.value = false;
      result.error_log.push_back(
          CreateError({"Failed to delete file: ", file_path.AsUTF8Unsafe()}));
    }
  }

  return result;
}

// Returns the list of resulting files copied to the destination directory, or a
// string error. This method does nothing if the source_dir is empty.
TypedResult<bool> MaybeReplaceFilesInDirectoryDeleteRemaining(
    const base::FilePath& source_dir,
    const base::FilePath& dest_dir) {
  absl::flat_hash_set<base::FilePath> replaced_files;

  if (!base::DirectoryExists(source_dir)) {
    return {.value = true};
  }

  base::File::Error file_error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(dest_dir, &file_error)) {
    return {.error_log = {CreateError(
                {"Failed to ensure directory ", dest_dir.AsUTF8Unsafe(),
                 " was created: ", base::File::ErrorToString(file_error)})}};
  }

  base::FileEnumerator pending_iter(source_dir,
                                    /*recursive=*/false,
                                    base::FileEnumerator::FILES);
  std::vector<std::string> error_messages;

  pending_iter.ForEach([&](const base::FilePath& source_file) {
    // The BaseName() ensures we only copy the file name itself (e.g.,
    // "icon.png") and not the full path from 'source_dir'.
    // Example: If source_file is '/tmp/pending_icons/any/96.png'
    //          and dest_dir is '/data/icons/any'
    //          source_file.BaseName() returns '96.png'.
    //          dest_file becomes '/data/icons/any/96.png'
    base::FilePath dest_file = dest_dir.Append(source_file.BaseName());
    if (base::CopyFile(source_file, dest_file)) {
      replaced_files.insert(std::move(dest_file));
      return;
    }

    error_messages.push_back(
        CreateError({"Could not copy file from ", source_file.AsUTF8Unsafe(),
                     " to ", dest_file.AsUTF8Unsafe()}));
  });

  if (!error_messages.empty()) {
    return {.error_log = std::move(error_messages)};
  }

  // Delete files in dest_dir that were NOT replaced.
  TypedResult<bool> delete_result =
      DeleteOtherFilesInDir(dest_dir, replaced_files);
  if (delete_result.HasErrors()) {
    return delete_result;
  }

  return {.value = true};
}

// This method takes two directories that are expected to be in the
// 'manifest icon directory' format, where there are sub-directories per icon
// purpose. This method will ensure the icon directories in the destination
// directory contain (and only contains) the respective icons from the source
// directory.
TypedResult<bool> ReplacePurposedIcons(const base::FilePath& source_dir,
                                       const base::FilePath& dest_dir) {
  for (const IconPurpose& purpose : kIconPurposes) {
    base::FilePath purpose_relative_dir =
        GetRelativeDirectoryForPurpose(purpose);

    base::FilePath source_purpose_dir = source_dir.Append(purpose_relative_dir);
    base::FilePath dest_purpose_dir = dest_dir.Append(purpose_relative_dir);

    auto replace_result = MaybeReplaceFilesInDirectoryDeleteRemaining(
        source_purpose_dir, dest_purpose_dir);
    if (replace_result.HasErrors()) {
      return replace_result;
    }
  }

  return {.value = true};
}

// Performs blocking I/O. May be called on another thread.
// Copies all pending update icons (trusted and manifest) into their
// corresponding non-pending directories. Returns false if any copy operation
// has failed.
TypedResult<bool> OverwriteAppIconsFromPendingIconsBlocking(
    const base::FilePath& app_manifest_resources_directory,
    const base::FilePath& app_trusted_icons_subdir,
    const base::FilePath& app_pending_manifest_icons_subdir,
    const base::FilePath& app_pending_trusted_icons_subdir) {
  base::File::Error file_error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(app_manifest_resources_directory,
                                        &file_error)) {
    return {.error_log = {CreateError(
                {"Failed to ensure directory ",
                 app_manifest_resources_directory.AsUTF8Unsafe(),
                 " was created: ", base::File::ErrorToString(file_error)})}};
  }

  if (!base::DirectoryExists(app_pending_manifest_icons_subdir)) {
    return {.error_log = {CreateError(
                {"App pending manifest directory not found: ",
                 app_manifest_resources_directory.AsUTF8Unsafe()})}};
  }

  if (!base::CreateDirectoryAndGetError(app_trusted_icons_subdir,
                                        &file_error)) {
    return {.error_log = {CreateError(
                {"Failed to ensure directory ",
                 app_trusted_icons_subdir.AsUTF8Unsafe(),
                 " was created: ", base::File::ErrorToString(file_error)})}};
  }

  if (!base::DirectoryExists(app_pending_trusted_icons_subdir)) {
    return {.error_log = {CreateError(
                {"App pending trusted directory not found: ",
                 app_manifest_resources_directory.AsUTF8Unsafe()})}};
  }

  TypedResult<bool> manifest_result = ReplacePurposedIcons(
      app_pending_manifest_icons_subdir, app_manifest_resources_directory);
  if (manifest_result.HasErrors()) {
    return manifest_result;
  }

  TypedResult<bool> trusted_result = ReplacePurposedIcons(
      app_pending_trusted_icons_subdir, app_trusted_icons_subdir);
  if (trusted_result.HasErrors()) {
    return trusted_result;
  }

  return {.value = true};
}

// Performs blocking I/O. May be called on another thread.
// Deletes a directory path and returns a result indicating success or failure
// with the path included in the error log on failure.
TypedResult<bool> DeleteDirectoryAndGetResultBlocking(
    base::FilePath file_path) {
  if (!base::DeletePathRecursively(file_path)) {
    return {.error_log = {CreateError(
                {"Failed to delete directory: ", file_path.AsUTF8Unsafe()})}};
  }
  return {.value = true};
}

// `web_apps_directory` is the path to the directory where all web app data is
// stored for the relevant profile.
base::FilePath GetManifestResourcesShortcutsMenuIconFileName(
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    IconPurpose purpose,
    int index,
    int icon_size_px) {
  const base::FilePath manifest_shortcuts_menu_icons_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory, app_id)
          .Append(GetAppShortcutsMenuIconsRelativeDirectory(purpose));
  const base::FilePath manifest_shortcuts_menu_icon_dir =
      manifest_shortcuts_menu_icons_dir.AppendASCII(
          base::NumberToString(index));

  return manifest_shortcuts_menu_icon_dir.AppendASCII(
      base::NumberToString(icon_size_px) + ".png");
}

base::FilePath GetIconFilePathFromReadConfiguration(
    const base::FilePath& web_apps_directory,
    const IconId& icon_id,
    ReadConfiguration read_configuration) {
  switch (read_configuration) {
    case ReadConfiguration::kManifestIcons:
      return GetIconFileName(web_apps_directory, icon_id);
    case ReadConfiguration::kTrustedIcons:
      return GetIconsFileNameForChildDirectory(
          web_apps_directory, base::FilePath(kTrustedIconFolderName), icon_id);
    case ReadConfiguration::kPendingTrustedIcons:
      return GetIconsFileNameForChildDirectory(
          web_apps_directory, base::FilePath(kPendingTrustedIconFolderName),
          icon_id);
  }
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
TypedResult<SkBitmap> ReadIconBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                       const base::FilePath& web_apps_directory,
                                       const IconId& icon_id,
                                       ReadConfiguration read_configuration) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadIconBlocking");
  base::FilePath icon_file = GetIconFilePathFromReadConfiguration(
      web_apps_directory, icon_id, read_configuration);
  auto icon_data = base::MakeRefCounted<base::RefCountedString>();
  if (!utils->ReadFileToString(icon_file, &icon_data->as_string())) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()},
                read_configuration == ReadConfiguration::kTrustedIcons)}};
  }

  TypedResult<SkBitmap> result;
  result.value = gfx::PNGCodec::Decode(*icon_data);
  if (result.value.isNull()) {
    return {.error_log = {CreateError({"Could not decode icon data for file: ",
                                       icon_file.AsUTF8Unsafe()})}};
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
// Returns null base::Time if any errors occurred.
TypedResult<base::Time> ReadIconTimeBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    base::FilePath icon_file,
    bool read_trusted_icon) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadIconTimeBlocking");
  base::File::Info file_info;
  if (!utils->GetFileInfo(icon_file, &file_info)) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()},
                read_trusted_icon)}};
  }

  TypedResult<base::Time> access_time;
  access_time.value = base::Time();
  if (!file_info.last_modified.is_null()) {
    access_time.value = file_info.last_modified;
  }
  return access_time;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
TypedResult<SkBitmap> ReadShortcutsMenuIconBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    IconPurpose purpose,
    int index,
    int icon_size_px) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadShortcutsMenuIconBlocking");
  base::FilePath icon_file = GetManifestResourcesShortcutsMenuIconFileName(
      web_apps_directory, app_id, purpose, index, icon_size_px);

  std::string icon_data;

  if (!utils->ReadFileToString(icon_file, &icon_data)) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()})}};
  }

  TypedResult<SkBitmap> result;
  result.value = gfx::PNGCodec::Decode(base::as_byte_span(icon_data));
  if (result.value.isNull()) {
    return {.error_log = {CreateError({"Could not decode icon data for file: ",
                                       icon_file.AsUTF8Unsafe()})}};
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty map if any errors occurred.
TypedResult<SizeToBitmap> ReadIconAndResizeBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const IconId& icon_id,
    SquareSizePx target_icon_size_px,
    bool is_trusted) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadIconAndResizeBlocking");
  TypedResult<SizeToBitmap> result;

  TypedResult<SkBitmap> read_result =
      ReadIconBlocking(std::move(utils), web_apps_directory, icon_id,
                       is_trusted ? ReadConfiguration::kTrustedIcons
                                  : ReadConfiguration::kManifestIcons);
  if (read_result.HasErrors())
    return {.error_log = std::move(read_result.error_log)};

  SkBitmap source = std::move(read_result.value);
  SkBitmap target;

  if (icon_id.size != target_icon_size_px) {
    target = skia::ImageOperations::Resize(
        source, skia::ImageOperations::RESIZE_BEST, target_icon_size_px,
        target_icon_size_px);
  } else {
    target = std::move(source);
  }

  result.value[target_icon_size_px] = std::move(target);
  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<IconMetadataFromDisk> ReadIconsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    IconPurpose purpose,
    const std::vector<SquareSizePx>& icon_sizes,
    bool read_trusted_icons) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadIconsBlocking");
  TypedResult<IconMetadataFromDisk> result;

  for (SquareSizePx icon_size_px : icon_sizes) {
    IconId icon_id(app_id, purpose, icon_size_px);
    TypedResult<SkBitmap> read_result = ReadIconBlocking(
        utils, web_apps_directory, icon_id,
        read_trusted_icons ? ReadConfiguration::kTrustedIcons
                           : ReadConfiguration::kManifestIcons);
    base::Extend(result.error_log, std::move(read_result.error_log));
    if (!read_result.value.empty()) {
      result.value.icons_map[icon_size_px] = std::move(read_result.value);
      result.value.purpose = purpose;
    }
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<IconMetadataForUpdate> ReadIconsForUpdateBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    std::optional<IconPurpose> purpose_for_pending_info,
    IconPurpose purpose_for_current_trusted_icon,
    SquareSizePx icon_size) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadIconsForUpdateBlocking");
  TypedResult<IconMetadataForUpdate> result;

  // First, read the trusted icon to be shown on the update dialog (the "from")
  // icon.
  IconId icon_id_trusted(app_id, purpose_for_current_trusted_icon, icon_size);
  TypedResult<SkBitmap> read_result =
      ReadIconBlocking(utils, web_apps_directory, icon_id_trusted,
                       ReadConfiguration::kTrustedIcons);

  // This guarantees that `from` icons are always populated, because there is a
  // chance that the app might not have had trusted icons.
  if (read_result.HasErrors()) {
    read_result.error_log = {};
    read_result = ReadIconBlocking(utils, web_apps_directory,
                                   IconId(app_id, IconPurpose::ANY, icon_size),
                                   ReadConfiguration::kManifestIcons);
  }

  // Return if there are no manifest icons as well, this is a legitimate error
  // case.
  if (read_result.HasErrors()) {
    return {.error_log = std::move(read_result.error_log)};
  }

  result.value.from_icon = std::move(read_result.value);
  result.value.from_icon_purpose = purpose_for_current_trusted_icon;

  // Second, read the "to" icon for the update dialog if there is one.
  if (purpose_for_pending_info.has_value()) {
    IconId icon_id_pending(app_id, *purpose_for_pending_info, icon_size);
    TypedResult<SkBitmap> pending_read_result =
        ReadIconBlocking(utils, web_apps_directory, icon_id_pending,
                         ReadConfiguration::kPendingTrustedIcons);
    if (pending_read_result.HasErrors()) {
      return {.error_log = std::move(pending_read_result.error_log)};
    }

    result.value.to_icon = std::move(pending_read_result.value);
    result.value.to_icon_purpose = purpose_for_pending_info;
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<IconMetadataFromDisk> ReadTrustedIconsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    IconPurpose purpose_for_fallback,
    const std::vector<SquareSizePx>& icon_sizes) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadTrustedIconsBlocking");
  TypedResult<IconMetadataFromDisk> result;

// First check for maskable icons available on Mac and ChromeOS.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  result = ReadIconsBlocking(utils, web_apps_directory, app_id,
                             IconPurpose::MASKABLE, icon_sizes,
                             /*read_trusted_icons=*/true);
  if (!result.value.icons_map.empty()) {
    RecordTrustedIconsReadResult(/*trusted_icon_used=*/true);
    result.value.purpose = IconPurpose::MASKABLE;
    return result;
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

  // Check for `ANY` trusted icons. For MacOS and ChromeOS,
  // this behaves as a fallback to check for `ANY` icons if no maskable
  // ones have been found.
  result = ReadIconsBlocking(utils, web_apps_directory, app_id,
                             IconPurpose::ANY, icon_sizes,
                             /*read_trusted_icons=*/true);
  if (!result.value.icons_map.empty()) {
    RecordTrustedIconsReadResult(/*trusted_icon_used=*/true);
    result.value.purpose = IconPurpose::ANY;
    return result;
  }

  // If no icons has been found in the trusted icon
  // directory, then read from the top level icons directory storing the
  // manifest icon bitmaps.
  result = ReadIconsBlocking(utils, web_apps_directory, app_id,
                             purpose_for_fallback, icon_sizes,
                             /*read_trusted_icons=*/false);
  if (!result.value.icons_map.empty()) {
    result.value.purpose = purpose_for_fallback;
  }

  RecordTrustedIconsReadResult(/*trusted_icon_used=*/false);
  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<base::flat_map<SquareSizePx, base::Time>>
ReadIconsLastUpdateTimeBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                const base::FilePath& web_apps_directory,
                                const webapps::AppId& app_id,
                                IconPurpose purpose,
                                const std::vector<SquareSizePx>& icon_sizes,
                                bool consider_trusted_icons) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadIconsLastUpdateTimeBlocking");
  TypedResult<base::flat_map<SquareSizePx, base::Time>> result;

  for (SquareSizePx icon_size_px : icon_sizes) {
    IconId icon_id(app_id, purpose, icon_size_px);
    base::FilePath icon_file =
        consider_trusted_icons
            ? GetIconsFileNameForChildDirectory(
                  web_apps_directory, base::FilePath(kTrustedIconFolderName),
                  icon_id)
            : GetIconFileName(web_apps_directory, icon_id);
    TypedResult<base::Time> read_result =
        ReadIconTimeBlocking(utils, icon_file, consider_trusted_icons);
    base::Extend(result.error_log, std::move(read_result.error_log));
    if (!read_result.value.is_null())
      result.value[icon_size_px] = std::move(read_result.value);
  }

  return result;
}

TypedResult<WebAppIconManager::WebAppBitmaps> ReadAllIconsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    const std::map<IconPurpose, std::vector<SquareSizePx>>&
        manifest_icon_purpose_to_sizes,
    const std::map<IconPurpose, std::vector<SquareSizePx>>&
        trusted_icon_purpose_to_sizes) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadAllIconsBlocking");
  TypedResult<WebAppIconManager::WebAppBitmaps> result;

  // Read manifest icons (untrusted) first.
  for (const auto& purpose_sizes : manifest_icon_purpose_to_sizes) {
    TypedResult<IconMetadataFromDisk> read_result =
        ReadIconsBlocking(utils, web_apps_directory, app_id,
                          purpose_sizes.first, purpose_sizes.second,
                          /*read_trusted_icons=*/false);
    base::Extend(result.error_log, std::move(read_result.error_log));
    result.value.manifest_icons.SetBitmapsForPurpose(
        purpose_sizes.first, std::move(read_result.value.icons_map));
  }

  // Read trusted icons next.
  for (const auto& purpose_sizes : trusted_icon_purpose_to_sizes) {
    TypedResult<IconMetadataFromDisk> read_result =
        ReadIconsBlocking(utils, web_apps_directory, app_id,
                          purpose_sizes.first, purpose_sizes.second,
                          /*read_trusted_icons=*/true);
    base::Extend(result.error_log, std::move(read_result.error_log));
    result.value.trusted_icons.SetBitmapsForPurpose(
        purpose_sizes.first, std::move(read_result.value.icons_map));
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
TypedResult<ShortcutsMenuIconBitmaps> ReadShortcutsMenuIconsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadShortcutsMenuIconsBlocking");
  TypedResult<ShortcutsMenuIconBitmaps> results;
  int curr_index = 0;
  for (const auto& item_info : shortcuts_menu_item_infos) {
    IconBitmaps result;

    for (IconPurpose purpose : kIconPurposes) {
      SizeToBitmap bitmaps;

      for (SquareSizePx icon_size_px :
           item_info.downloaded_icon_sizes.GetSizesForPurpose(purpose)) {
        TypedResult<SkBitmap> read_result =
            ReadShortcutsMenuIconBlocking(utils, web_apps_directory, app_id,
                                          purpose, curr_index, icon_size_px);
        base::Extend(results.error_log, std::move(read_result.error_log));
        if (!read_result.value.empty())
          bitmaps[icon_size_px] = std::move(read_result.value);
      }

      result.SetBitmapsForPurpose(purpose, std::move(bitmaps));
    }

    ++curr_index;
    // We always push_back (even when result is empty) to keep a given
    // std::map's index in sync with that of its corresponding shortcuts menu
    // item.
    results.value.push_back(std::move(result));
  }
  CHECK_EQ(shortcuts_menu_item_infos.size(), results.value.size());
  return results;
}

TypedResult<WebAppIconManager::ShortcutIconDataVector>
ReadShortcutMenuIconsWithTimestampBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_icon_infos) {
  TRACE_EVENT0(
      "ui", "web_app_icon_manager::ReadShortcutMenuIconsWithTimestampBlocking");
  TypedResult<WebAppIconManager::ShortcutIconDataVector> results;
  int curr_index = 0;
  for (const auto& icon_info : shortcuts_menu_icon_infos) {
    WebAppIconManager::ShortcutMenuIconTimes data;
    for (IconPurpose purpose : kIconPurposes) {
      base::flat_map<SquareSizePx, base::Time> bitmap_with_time;
      for (SquareSizePx icon_size_px :
           icon_info.downloaded_icon_sizes.GetSizesForPurpose(purpose)) {
        base::FilePath file_name =
            GetManifestResourcesShortcutsMenuIconFileName(
                web_apps_directory, app_id, purpose, curr_index, icon_size_px);
        TypedResult<base::Time> read_result =
            ReadIconTimeBlocking(utils, file_name, /*read_trusted_icon=*/false);
        base::Extend(results.error_log, std::move(read_result.error_log));
        if (!read_result.value.is_null()) {
          bitmap_with_time[icon_size_px] = std::move(read_result.value);
        }
      }
      data[purpose] = bitmap_with_time;
    }
    ++curr_index;
    // We always push_back (even when result is empty) to keep a given
    // std::map's index in sync with that of its corresponding shortcuts menu
    // item.
    results.value.push_back(std::move(data));
  }
  CHECK_EQ(shortcuts_menu_icon_infos.size(), results.value.size());
  return results;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty vector if any errors occurred.
TypedResult<std::vector<uint8_t>> ReadCompressedIconBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const IconId& icon_id,
    bool is_trusted) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ReadCompressedIconBlocking");
  base::FilePath icon_file =
      is_trusted ? GetIconsFileNameForChildDirectory(
                       web_apps_directory,
                       base::FilePath(kTrustedIconFolderName), icon_id)
                 : GetIconFileName(web_apps_directory, icon_id);

  std::string icon_data;
  if (!utils->ReadFileToString(icon_file, &icon_data)) {
    return {.error_log = {CreateError(
                {"Could not read icon file: ", icon_file.AsUTF8Unsafe()},
                is_trusted)}};
  }

  // Copy data: we can't std::move std::string into std::vector.
  return {.value = {icon_data.begin(), icon_data.end()}};
}

WebAppIconManager::IconFilesCheck CheckForEmptyOrMissingIconFilesBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    base::flat_map<IconPurpose, SortedSizesPx> manifest_icon_purpose_to_sizes,
    base::flat_map<IconPurpose, SortedSizesPx> trusted_icon_purpose_to_sizes) {
  TRACE_EVENT0("ui",
               "web_app_icon_manager::CheckForEmptyOrMissingIconFilesBlocking");
  WebAppIconManager::IconFilesCheck result;

  // First, parse all the manifest icon files.
  for (const auto& [purpose, square_sizes] : manifest_icon_purpose_to_sizes) {
    for (SquareSizePx size : square_sizes) {
      base::FilePath icon_path =
          GetIconFileName(web_apps_directory, IconId(app_id, purpose, size));
      base::File::Info file_info;
      if (utils->GetFileInfo(icon_path, &file_info)) {
        if (file_info.size == 0)
          ++result.empty;
      } else {
        ++result.missing;
      }
    }
  }

  // Second, parse all the trusted icon files.
  for (const auto& [purpose, square_sizes] : trusted_icon_purpose_to_sizes) {
    for (SquareSizePx size : square_sizes) {
      base::FilePath icon_path = GetIconsFileNameForChildDirectory(
          web_apps_directory, base::FilePath(kTrustedIconFolderName),
          IconId(app_id, purpose, size));
      base::File::Info file_info;
      if (utils->GetFileInfo(icon_path, &file_info)) {
        if (file_info.size == 0) {
          ++result.empty;
        }
      } else {
        ++result.missing;
      }
    }
  }
  return result;
}

gfx::ImageSkia ConvertFaviconBitmapsToImageSkia(
    const SizeToBitmap& icon_bitmaps) {
  TRACE_EVENT0("ui", "web_app_icon_manager::ConvertFaviconBitmapsToImageSkia");
  gfx::ImageSkia image_skia;

  for (const auto& [size, bitmap] : icon_bitmaps) {
    if (bitmap.empty() || size < gfx::kFaviconSize) {
      continue;
    }
    SkBitmap bitmap_to_resize = bitmap;
    // Resize |bitmap_to_resize| to match |gfx::kFaviconSize|.
    if (bitmap_to_resize.width() != gfx::kFaviconSize) {
      bitmap_to_resize = skia::ImageOperations::Resize(
          bitmap_to_resize, skia::ImageOperations::RESIZE_BEST,
          gfx::kFaviconSize, gfx::kFaviconSize);
    }
    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap_to_resize, 1.0f));
    break;
  }

  const int largest_favicon_scale = 4;
  const int largest_favicon_size = gfx::kFaviconSize * largest_favicon_scale;
  for (const auto& [size, bitmap] : icon_bitmaps) {
    // Don't add the gfx::kFaviconSize sized icon again, and ensure we only
    // add icons smaller or equal to the scale.
    if (bitmap.empty() || size <= gfx::kFaviconSize) {
      continue;
    }
    // If we have a large icon, we should resize it.
    if (size > largest_favicon_size) {
      SkBitmap bitmap_to_resize = bitmap;
      bitmap_to_resize = skia::ImageOperations::Resize(
          bitmap_to_resize, skia::ImageOperations::RESIZE_BEST,
          largest_favicon_size, largest_favicon_size);
      image_skia.AddRepresentation(
          gfx::ImageSkiaRep(bitmap_to_resize, largest_favicon_scale));
    } else {
      image_skia.AddRepresentation(gfx::ImageSkiaRep(
          bitmap, static_cast<float>(size) / gfx::kFaviconSize));
    }
  }

  return image_skia;
}

// A utility that manages writing icons to disk for a single app. Should only be
// used on an I/O thread.
class WriteIconsJob {
 public:
  static TypedResult<bool> WriteIconsBlocking(
      scoped_refptr<FileUtilsWrapper> utils,
      base::FilePath web_apps_directory,
      webapps::AppId app_id,
      IconBitmaps icon_bitmaps,
      IconBitmaps trusted_icon_bitmaps,
      IconBitmaps pending_trusted_icon_bitmaps,
      IconBitmaps pending_manifest_icon_bitmaps,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps,
      IconsMap other_icons) {
    TRACE_EVENT0("ui",
                 "web_app_icon_manager::WriteIconsJob::WriteIconsBlocking");
    WriteIconsJob job(
        std::move(utils), std::move(web_apps_directory), std::move(app_id),
        std::move(icon_bitmaps), std::move(trusted_icon_bitmaps),
        std::move(pending_trusted_icon_bitmaps),
        std::move(pending_manifest_icon_bitmaps),
        std::move(shortcuts_menu_icon_bitmaps), std::move(other_icons));
    return job.Execute();
  }

  WriteIconsJob(const WriteIconsJob& other) = delete;
  WriteIconsJob& operator=(const WriteIconsJob& other) = delete;

 private:
  WriteIconsJob(scoped_refptr<FileUtilsWrapper> utils,
                base::FilePath web_apps_directory,
                webapps::AppId app_id,
                IconBitmaps icon_bitmaps,
                IconBitmaps trusted_icon_bitmaps,
                IconBitmaps pending_trusted_icon_bitmaps,
                IconBitmaps pending_manifest_icon_bitmaps,
                ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps,
                IconsMap other_icons)
      : utils_(std::move(utils)),
        web_apps_directory_(std::move(web_apps_directory)),
        app_id_(std::move(app_id)),
        icon_bitmaps_(std::move(icon_bitmaps)),
        trusted_icon_bitmaps_(std::move(trusted_icon_bitmaps)),
        pending_trusted_icon_bitmaps_(std::move(pending_trusted_icon_bitmaps)),
        pending_manifest_icon_bitmaps_(
            std::move(pending_manifest_icon_bitmaps)),
        shortcuts_menu_icon_bitmaps_(std::move(shortcuts_menu_icon_bitmaps)),
        other_icons_(std::move(other_icons)) {}
  ~WriteIconsJob() = default;

  TypedResult<bool> Execute() {
    TRACE_EVENT0("ui", "web_app_icon_manager::WriteIconsJob::Execute");
    TypedResult<bool> result(true);
    // Write product icons directly in the app's directory.
    if (!icon_bitmaps_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteIcons,
                              base::Unretained(this), icon_bitmaps_),
          /*subdir_for_icons=*/{});
      if (result.HasErrors()) {
        return result;
      }
    }

    if (!trusted_icon_bitmaps_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteIcons,
                              base::Unretained(this), trusted_icon_bitmaps_),
          /*subdir_for_icons=*/base::FilePath(kTrustedIconFolderName));
      if (result.HasErrors()) {
        return result;
      }
    }

    if (!pending_trusted_icon_bitmaps_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteIcons,
                              base::Unretained(this),
                              pending_trusted_icon_bitmaps_),
          /*subdir_for_icons=*/base::FilePath(kPendingTrustedIconFolderName));
      if (result.HasErrors()) {
        return result;
      }
    }

    if (!pending_manifest_icon_bitmaps_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteIcons,
                              base::Unretained(this),
                              pending_manifest_icon_bitmaps_),
          /*subdir_for_icons=*/base::FilePath(kPendingManifestIconFolderName));
      if (result.HasErrors()) {
        return result;
      }
    }

    if (!shortcuts_menu_icon_bitmaps_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteShortcutsMenuIcons,
                              base::Unretained(this)),
          /*subdir_for_icons=*/GetAppShortcutsMenuIconsRelativeDirectory(
              IconPurpose::ANY));

      if (result.HasErrors())
        return result;
    }

    if (!other_icons_.empty()) {
      result = AtomicallyWriteIcons(
          base::BindRepeating(&WriteIconsJob::WriteOtherIcons,
                              base::Unretained(this)),
          /*subdir_for_icons=*/
          GetOtherIconsRelativeDirectory());
    }

    return result;
  }

  // Manages writing a set of icons to a particular location on disk, making a
  // best-effort to make it all-or-nothing. Returns true if no errors occurred.
  // This is used for several kinds of icon data. The passed callbacks allow for
  // varying the implementation based on data type. `write_icons_callback` is
  // expected to write the icons data under the passed base directory.
  // `subdir_for_icons` is a relative FilePath representing a directory which
  // holds all the data written by `write_icons_callback`. The path is relative
  // to the app's manifest resources directory.
  TypedResult<bool> AtomicallyWriteIcons(
      const base::RepeatingCallback<TypedResult<bool>(
          const base::FilePath& path,
          const base::FilePath& subdir_path)>& write_icons_callback,
      const base::FilePath& subdir_for_icons) {
    TRACE_EVENT0("ui",
                 "web_app_icon_manager::WriteIconsJob::AtomicallyWriteIcons");
    DCHECK(!subdir_for_icons.IsAbsolute());
    // Create the temp directory under the web apps root.
    // This guarantees it is on the same file system as the WebApp's eventual
    // install target.
    base::FilePath temp_dir = GetWebAppsTempDirectory(web_apps_directory_);
    TypedResult<bool> create_result = CreateDirectory(temp_dir);
    if (create_result.HasErrors())
      return create_result;

    base::ScopedTempDir app_temp_dir;
    if (!app_temp_dir.CreateUniqueTempDirUnderPath(temp_dir)) {
      return {
          .error_log = {CreateError({"Could not create temp directory under: ",
                                     temp_dir.AsUTF8Unsafe()})}};
    }

    base::FilePath manifest_resources_directory =
        GetManifestResourcesDirectory(web_apps_directory_);
    create_result = CreateDirectory(manifest_resources_directory);
    if (create_result.HasErrors())
      return create_result;

    TypedResult<bool> write_result =
        write_icons_callback.Run(app_temp_dir.GetPath(), subdir_for_icons);
    if (write_result.HasErrors())
      return write_result;

    base::FilePath app_dir =
        GetManifestResourcesDirectoryForApp(web_apps_directory_, app_id_);
    base::FilePath final_icons_dir = app_dir.Append(subdir_for_icons);
    // Create app_dir if it doesn't already exist. We'll need this for
    // WriteShortcutsMenuIconsData unittests.
    if (final_icons_dir != app_dir) {
      create_result = CreateDirectory(app_dir);
      if (create_result.HasErrors())
        return create_result;
    }

    // Delete the destination. Needed for update. Ignore the result.
    utils_->DeleteFileRecursively(final_icons_dir);

    base::FilePath temp_icons_dir =
        app_temp_dir.GetPath().Append(subdir_for_icons);
    // Commit: move whole icons data dir to final destination in one mv
    // operation.
    if (!utils_->Move(temp_icons_dir, final_icons_dir)) {
      return {.error_log = {CreateError(
                  {"Could not move: ", temp_icons_dir.AsUTF8Unsafe(),
                   " to: ", final_icons_dir.AsUTF8Unsafe()})}};
    }

    return {.value = true};
  }

  TypedResult<bool> WriteIcons(const IconBitmaps& icon_bitmaps,
                               const base::FilePath& base_dir,
                               const base::FilePath& subdir_for_icons) {
    TRACE_EVENT0("ui", "web_app_icon_manager::WriteIconsJob::WriteIcons");
    for (IconPurpose purpose : kIconPurposes) {
      base::FilePath icons_dir =
          base_dir.Append(subdir_for_icons)
              .Append(GetRelativeDirectoryForPurpose(purpose));

      auto create_result = CreateDirectory(icons_dir);
      if (create_result.HasErrors()) {
        return create_result;
      }

      for (const std::pair<const SquareSizePx, SkBitmap>& icon_bitmap :
           icon_bitmaps.GetBitmapsForPurpose(purpose)) {
        TypedResult<bool> write_result =
            EncodeAndWriteIcon(icons_dir, icon_bitmap.second);
        if (write_result.HasErrors()) {
          return write_result;
        }
      }
    }
    return {.value = true};
  }

  // Writes shortcuts menu icons files to the Shortcut Icons directory. Creates
  // a new directory per shortcut item using its index in the vector.
  TypedResult<bool> WriteShortcutsMenuIcons(
      const base::FilePath& app_manifest_resources_directory,
      const base::FilePath&) {
    TRACE_EVENT0(
        "ui", "web_app_icon_manager::WriteIconsJob::WriteShortcutsMenuIcons");
    for (IconPurpose purpose : kIconPurposes) {
      const base::FilePath shortcuts_menu_icons_dir =
          app_manifest_resources_directory.Append(
              GetAppShortcutsMenuIconsRelativeDirectory(purpose));
      auto create_result = CreateDirectory(shortcuts_menu_icons_dir);
      if (create_result.HasErrors())
        return create_result;

      int shortcut_index = -1;
      for (const IconBitmaps& icon_bitmaps : shortcuts_menu_icon_bitmaps_) {
        ++shortcut_index;
        const SizeToBitmap& bitmaps =
            icon_bitmaps.GetBitmapsForPurpose(purpose);
        if (bitmaps.empty())
          continue;

        const base::FilePath shortcuts_menu_icon_dir =
            shortcuts_menu_icons_dir.AppendASCII(
                base::NumberToString(shortcut_index));
        create_result = CreateDirectory(shortcuts_menu_icon_dir);
        if (create_result.HasErrors())
          return create_result;

        for (const std::pair<const SquareSizePx, SkBitmap>& icon_bitmap :
             bitmaps) {
          TypedResult<bool> write_result =
              EncodeAndWriteIcon(shortcuts_menu_icon_dir, icon_bitmap.second);
          if (write_result.HasErrors())
            return write_result;
        }
      }
    }
    return {.value = true};
  }

  TypedResult<bool> WriteOtherIcons(
      const base::FilePath& app_manifest_resources_directory,
      const base::FilePath& subdir_path) {
    TRACE_EVENT0("ui", "web_app_icon_manager::WriteIconsJob::WriteOtherIcons");
    const base::FilePath general_icons_dir =
        app_manifest_resources_directory.Append(subdir_path);
    auto create_result = CreateDirectory(general_icons_dir);
    if (create_result.HasErrors())
      return create_result;

    for (const std::pair<const GURL, std::vector<SkBitmap>>& entry :
         other_icons_) {
      const base::FilePath subdir =
          general_icons_dir.AppendASCII(GetDirectoryNameForUrl(entry.first));
      create_result = CreateDirectory(subdir);
      if (create_result.HasErrors())
        return create_result;

      const std::vector<SkBitmap>& icon_bitmaps = entry.second;
      for (const SkBitmap& icon_bitmap : icon_bitmaps) {
        TypedResult<bool> write_result =
            EncodeAndWriteIcon(subdir, icon_bitmap);
        if (write_result.HasErrors())
          return write_result;
      }
    }
    return {.value = true};
  }

  TypedResult<bool> CreateDirectory(const base::FilePath& path) {
    if (!utils_->CreateDirectory(path)) {
      return {.error_log = {CreateError(
                  {"Could not create directory: ", path.AsUTF8Unsafe()})}};
    }

    return {.value = true};
  }

  // Encodes `bitmap` as a PNG and writes to the given directory.
  TypedResult<bool> EncodeAndWriteIcon(const base::FilePath& icons_dir,
                                       const SkBitmap& bitmap) {
    TRACE_EVENT0("ui",
                 "web_app_icon_manager::WriteIconsJob::EncodeAndWriteIcon");
    DCHECK_NE(bitmap.colorType(), kUnknown_SkColorType);
    DCHECK_EQ(bitmap.width(), bitmap.height());
    base::FilePath icon_file =
        icons_dir.AppendASCII(base::StringPrintf("%i.png", bitmap.width()));

    std::optional<std::vector<uint8_t>> image_data =
        gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                          /*discard_transparency=*/false);
    if (!image_data) {
      return {.error_log = {CreateError({"Could not encode icon data for file ",
                                         icon_file.AsUTF8Unsafe()})}};
    }

    if (!utils_->WriteFile(icon_file, image_data.value())) {
      return {.error_log = {CreateError(
                  {"Could not write icon file: ", icon_file.AsUTF8Unsafe()})}};
    }

    return {.value = true};
  }

  scoped_refptr<FileUtilsWrapper> utils_;
  base::FilePath web_apps_directory_;
  webapps::AppId app_id_;
  IconBitmaps icon_bitmaps_;
  IconBitmaps trusted_icon_bitmaps_;
  IconBitmaps pending_trusted_icon_bitmaps_;
  IconBitmaps pending_manifest_icon_bitmaps_;
  ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps_;
  SkBitmap home_tab_icon_bitmap_;
  IconsMap other_icons_;
};

uint64_t AccumulateIconsSizeForApp(std::vector<base::FilePath> icon_paths) {
  uint64_t total_size = 0;

  for (const base::FilePath& icon_path : icon_paths) {
    std::optional<int64_t> file_size = base::GetFileSize(icon_path);
    if (file_size.has_value()) {
      total_size += file_size.value();
    }
  }
  return total_size;
}

base::FilePath GetAppPendingTrustedIconsDir(
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id) {
  return web_app::GetManifestResourcesDirectoryForApp(web_apps_directory,
                                                      app_id)
      .Append(kPendingTrustedIconFolderName);
}

base::FilePath GetAppPendingManifestIconsDir(
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id) {
  return web_app::GetManifestResourcesDirectoryForApp(web_apps_directory,
                                                      app_id)
      .Append(kPendingManifestIconFolderName);
}

}  // namespace

IconMetadataFromDisk::IconMetadataFromDisk() = default;
IconMetadataFromDisk::~IconMetadataFromDisk() = default;
IconMetadataFromDisk::IconMetadataFromDisk(
    IconMetadataFromDisk&& icon_metadata) = default;
IconMetadataFromDisk& IconMetadataFromDisk::operator=(
    IconMetadataFromDisk&& icon_metadata) = default;

IconMetadataForUpdate::IconMetadataForUpdate() = default;
IconMetadataForUpdate::~IconMetadataForUpdate() = default;
IconMetadataForUpdate::IconMetadataForUpdate(
    IconMetadataForUpdate&& icon_metadata) = default;
IconMetadataForUpdate& IconMetadataForUpdate::operator=(
    IconMetadataForUpdate&& icon_metadata) = default;

WebAppIconManager::WebAppIconManager(Profile* profile)
    : icon_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  web_apps_directory_ = GetWebAppsRootDirectory(profile);
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo))
    error_log_ = std::make_unique<std::vector<std::string>>();
}

WebAppIconManager::~WebAppIconManager() = default;

// static
ReadIconMetadataCallback WebAppIconManager::BitmapsFromIconMetadataExtractor(
    base::OnceCallback<void(std::map<int, SkBitmap>)> icon_metadata_callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(std::map<int, SkBitmap>)>
             icon_metadata_callback,
         IconMetadataFromDisk icon_metadata_from_disk) {
        std::move(icon_metadata_callback)
            .Run(std::move(icon_metadata_from_disk.icons_map));
      },
      std::move(icon_metadata_callback));
}

void WebAppIconManager::WriteData(
    webapps::AppId app_id,
    IconBitmaps icon_bitmaps,
    IconBitmaps trusted_icon_bitmaps,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps,
    IconsMap other_icons_map,
    WriteDataCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::WriteData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &WriteIconsJob::WriteIconsBlocking, provider_->file_utils(),
          web_apps_directory_, std::move(app_id), std::move(icon_bitmaps),
          std::move(trusted_icon_bitmaps), IconBitmaps{}, IconBitmaps{},
          std::move(shortcuts_menu_icon_bitmaps), std::move(other_icons_map)),
      base::BindOnce(&LogErrorsCallCallback<bool>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::WritePendingIconData(
    webapps::AppId app_id,
    IconBitmaps pending_trusted_icon_bitmaps,
    IconBitmaps pending_manifest_icon_bitmaps,
    WriteDataCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::WriteData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteIconsJob::WriteIconsBlocking,
                     provider_->file_utils(), web_apps_directory_,
                     std::move(app_id), IconBitmaps{}, IconBitmaps{},
                     std::move(pending_trusted_icon_bitmaps),
                     std::move(pending_manifest_icon_bitmaps),
                     ShortcutsMenuIconBitmaps{}, IconsMap{}),
      base::BindOnce(&LogErrorsCallCallback<bool>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::DeleteData(webapps::AppId app_id,
                                   WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(DeleteDataBlocking, provider_->file_utils(),
                     web_apps_directory_, std::move(app_id)),
      std::move(callback));
}

void WebAppIconManager::SetProvider(base::PassKey<WebAppProvider>,
                                    WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppIconManager::Start() {
  TRACE_EVENT0("ui", "WebAppIconManager::Start");
  for (const webapps::AppId& app_id :
       provider_->registrar_unsafe().GetAppIds()) {
    ReadFavicon(app_id);

#if BUILDFLAG(IS_CHROMEOS)
    // Notifications use a monochrome icon.
    ReadMonochromeFavicon(app_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  install_manager_observation_.Observe(&provider_->install_manager());
}

void WebAppIconManager::Shutdown() {}

bool WebAppIconManager::HasIcons(const webapps::AppId& app_id,
                                 IconPurpose purpose,
                                 const SortedSizesPx& icon_sizes) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app)
    return false;

  return std::ranges::includes(web_app->downloaded_icon_sizes(purpose),
                               icon_sizes);
}

bool WebAppIconManager::HasTrustedIcons(const webapps::AppId& app_id,
                                        IconPurpose purpose,
                                        const SortedSizesPx& icon_sizes) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // There can never be monochrome trusted icons.
  if (purpose == IconPurpose::MONOCHROME) {
    return false;
  }

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    return false;
  }
  return std::ranges::includes(web_app->stored_trusted_icon_sizes(purpose),
                               icon_sizes);
}

std::optional<WebAppIconManager::IconSizeAndPurpose>
WebAppIconManager::FindIconMatchBigger(
    const webapps::AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size,
    bool skip_trusted_icons_for_favicons) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app)
    return std::nullopt;

  if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon) &&
      !skip_trusted_icons_for_favicons) {
    // Must iterate through purposes in order given.
    for (IconPurpose purpose : purposes) {
      if (purpose == IconPurpose::MONOCHROME) {
        continue;
      }
      // Must iterate sizes from smallest to largest.
      const SortedSizesPx& sizes = web_app->stored_trusted_icon_sizes(purpose);
      for (SquareSizePx size : sizes) {
        if (size >= min_size) {
          return IconSizeAndPurpose{size, purpose, /*is_trusted=*/true};
        }
      }
    }
  }

  // Must iterate through purposes in order given.
  for (IconPurpose purpose : purposes) {
    // Must iterate sizes from smallest to largest.
    const SortedSizesPx& sizes = web_app->downloaded_icon_sizes(purpose);
    for (SquareSizePx size : sizes) {
      if (size >= min_size)
        return IconSizeAndPurpose{size, purpose};
    }
  }

  return std::nullopt;
}

bool WebAppIconManager::HasSmallestIcon(
    const webapps::AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size) const {
  return FindIconMatchBigger(app_id, purposes, min_size).has_value();
}

void WebAppIconManager::ReadTrustedIconsWithFallbackToManifestIcons(
    const webapps::AppId& app_id,
    const SortedSizesPx& icon_sizes,
    IconPurpose purpose_for_fallback,
    ReadIconMetadataCallback callback) {
  TRACE_EVENT0(
      "ui", "WebAppIconManager::ReadTrustedIconsWithFallbackToManifestIcons");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(IconMetadataFromDisk());
    return;
  }

  // If the trusted icon usage is not enabled in the web applications system,
  // fallback to using the API to read manifest icons.
  if (!base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon)) {
    ReadUntrustedIcons(app_id, purpose_for_fallback, icon_sizes,
                       std::move(callback));
    return;
  }

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          ReadTrustedIconsBlocking, provider_->file_utils(),
          web_apps_directory_, app_id, purpose_for_fallback,
          std::vector<SquareSizePx>(icon_sizes.begin(), icon_sizes.end())),
      base::BindOnce(&LogErrorsCallCallback<IconMetadataFromDisk>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::ReadIconsForPendingUpdate(
    const webapps::AppId& app_id,
    SquareSizePx size,
    std::optional<IconPurpose> purpose_for_pending_info,
    ReadIconMetadataForUpdateCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadIconsForPendingUpdate");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(features::kWebAppPredictableAppUpdating)) {
    std::move(callback).Run(IconMetadataForUpdate());
    return;
  }

  if (!provider_->registrar_unsafe().GetAppById(app_id)) {
    std::move(callback).Run(IconMetadataForUpdate());
    return;
  }

  // Construct the purpose for the "from_icon" from the web app instead of
  // relying on an external input for correctness.
  IconPurpose purpose_for_current_trusted_icon = IconPurpose::ANY;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app->stored_trusted_icon_sizes(IconPurpose::MASKABLE).empty()) {
    purpose_for_current_trusted_icon = IconPurpose::MASKABLE;
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadIconsForUpdateBlocking, provider_->file_utils(),
                     web_apps_directory_, app_id, purpose_for_pending_info,
                     purpose_for_current_trusted_icon, size),
      base::BindOnce(&LogErrorsCallCallback<IconMetadataForUpdate>,
                     GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadAllShortcutMenuIconsWithTimestamp(
    const webapps::AppId& app_id,
    ShortcutIconDataCallback callback) {
  TRACE_EVENT0("ui",
               "WebAppIconManager::ReadAllShortcutMenuIconsWithTimestamp");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(ShortcutIconDataVector());
    return;
  }
  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadShortcutMenuIconsWithTimestampBlocking,
                     provider_->file_utils(), web_apps_directory_, app_id,
                     web_app->shortcuts_menu_item_infos()),
      base::BindOnce(&LogErrorsCallCallback<ShortcutIconDataVector>,
                     GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadIconsLastUpdateTime(
    const webapps::AppId& app_id,
    ReadIconsUpdateTimeCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadIconsLastUpdateTime");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(base::flat_map<SquareSizePx, base::Time>());
    return;
  }

  // Consider maskable trusted icons depending on OS, or fallback to reading
  // trusted icons of purpose `any`. If that is not available, fallback to
  // reading untrusted manifest icons.
  SortedSizesPx sizes_px{};
  bool consider_trusted_icons = false;
  IconPurpose purpose = IconPurpose::ANY;

  if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon)) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
    if (!web_app->stored_trusted_icon_sizes(IconPurpose::MASKABLE).empty()) {
      sizes_px = web_app->stored_trusted_icon_sizes(IconPurpose::MASKABLE);
      consider_trusted_icons = true;
      purpose = IconPurpose::MASKABLE;
    }
#endif  //  BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

    if (sizes_px.empty() &&
        !web_app->stored_trusted_icon_sizes(IconPurpose::ANY).empty()) {
      sizes_px = web_app->stored_trusted_icon_sizes(IconPurpose::ANY);
      consider_trusted_icons = true;
      purpose = IconPurpose::ANY;
    }
  }

  if (sizes_px.empty()) {
    // If no trusted icons are found, fallback to reading manifest icons.
    sizes_px = web_app->downloaded_icon_sizes(IconPurpose::ANY);
    consider_trusted_icons = false;
    purpose = IconPurpose::ANY;
  }

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          ReadIconsLastUpdateTimeBlocking, provider_->file_utils(),
          web_apps_directory_, app_id, purpose,
          std::vector<SquareSizePx>(sizes_px.begin(), sizes_px.end()),
          consider_trusted_icons),
      base::BindOnce(
          &LogErrorsCallCallback<base::flat_map<SquareSizePx, base::Time>>,
          GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::ReadAllIcons(const webapps::AppId& app_id,
                                     ReadIconBitmapsCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadAllIcons");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(WebAppBitmaps());
    return;
  }

  std::map<IconPurpose, std::vector<SquareSizePx>>
      manifest_icon_purposes_to_sizes;
  std::map<IconPurpose, std::vector<SquareSizePx>>
      trusted_icon_purposes_to_sizes;

  for (IconPurpose purpose : kIconPurposes) {
    const SortedSizesPx& sizes_px = web_app->downloaded_icon_sizes(purpose);
    manifest_icon_purposes_to_sizes[purpose] =
        std::vector<SquareSizePx>(sizes_px.begin(), sizes_px.end());
  }

  if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon)) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
    if (!web_app->stored_trusted_icon_sizes(IconPurpose::MASKABLE).empty()) {
      const SortedSizesPx& sizes_px =
          web_app->stored_trusted_icon_sizes(IconPurpose::MASKABLE);
      trusted_icon_purposes_to_sizes[IconPurpose::MASKABLE] =
          std::vector<SquareSizePx>(sizes_px.begin(), sizes_px.end());
    }
#endif  //  BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

    if (trusted_icon_purposes_to_sizes.empty()) {
      const SortedSizesPx& sizes_px =
          web_app->stored_trusted_icon_sizes(IconPurpose::ANY);
      trusted_icon_purposes_to_sizes[IconPurpose::ANY] =
          std::vector<SquareSizePx>(sizes_px.begin(), sizes_px.end());
    }
  }

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadAllIconsBlocking, provider_->file_utils(),
                     web_apps_directory_, app_id,
                     std::move(manifest_icon_purposes_to_sizes),
                     std::move(trusted_icon_purposes_to_sizes)),
      base::BindOnce(&LogErrorsCallCallback<WebAppBitmaps>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::ReadAllShortcutsMenuIcons(
    const webapps::AppId& app_id,
    ReadShortcutsMenuIconsCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadAllShortcutsMenuIcons");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(ShortcutsMenuIconBitmaps{});
    return;
  }

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadShortcutsMenuIconsBlocking, provider_->file_utils(),
                     web_apps_directory_, app_id,
                     web_app->shortcuts_menu_item_infos()),
      base::BindOnce(&LogErrorsCallCallback<ShortcutsMenuIconBitmaps>,
                     GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::GetIconsSizeForApp(
    const webapps::AppId& app_id,
    WebAppIconManager::GetIconsSizeCallback callback) const {
  std::vector<base::FilePath> icon_paths;

  // Populate manifest icon sizes.
  for (IconPurpose purpose : kIconPurposes) {
    for (SquareSizePx size : provider_->registrar_unsafe()
                                 .GetAppById(app_id)
                                 ->downloaded_icon_sizes(purpose)) {
      IconId icon_id(app_id, purpose, size);
      base::FilePath icon_path = GetIconFileName(web_apps_directory_, icon_id);
      icon_paths.push_back(icon_path);
    }
  }

  // Populate trusted icon sizes too if enabled.
  if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon)) {
    for (IconPurpose purpose : kIconPurposes) {
      if (purpose == IconPurpose::MONOCHROME) {
        continue;
      }
      for (SquareSizePx size : provider_->registrar_unsafe()
                                   .GetAppById(app_id)
                                   ->stored_trusted_icon_sizes(purpose)) {
        IconId icon_id(app_id, purpose, size);
        base::FilePath icon_path = GetIconsFileNameForChildDirectory(
            web_apps_directory_, base::FilePath(kTrustedIconFolderName),
            icon_id);
        icon_paths.push_back(icon_path);
      }
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&AccumulateIconsSizeForApp, std::move(icon_paths)),
      std::move(callback));
}

void WebAppIconManager::ReadSmallestIcon(
    const webapps::AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size_in_px,
    ReadIconWithPurposeCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadSmallestIcon");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, purposes, min_size_in_px);
  CHECK(best_icon.has_value());
  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  ReadIconCallback wrapped =
      base::BindOnce(std::move(callback), best_icon->purpose);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadIconBlocking, provider_->file_utils(),
                     web_apps_directory_, std::move(icon_id),
                     best_icon->is_trusted ? ReadConfiguration::kTrustedIcons
                                           : ReadConfiguration::kManifestIcons),
      base::BindOnce(&LogErrorsCallCallback<SkBitmap>, GetWeakPtr(),
                     std::move(wrapped)));
}

void WebAppIconManager::ReadSmallestCompressedIcon(
    const webapps::AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx min_size_in_px,
    ReadCompressedIconWithPurposeCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadSmallestCompressedIcon");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, purposes, min_size_in_px);
  CHECK(best_icon.has_value());
  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  ReadCompressedIconCallback wrapped =
      base::BindOnce(std::move(callback), best_icon->purpose);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadCompressedIconBlocking, provider_->file_utils(),
                     web_apps_directory_, std::move(icon_id),
                     best_icon->is_trusted),
      base::BindOnce(&LogErrorsCallCallback<std::vector<uint8_t>>, GetWeakPtr(),
                     std::move(wrapped)));
}

void WebAppIconManager::OverwriteAppIconsFromPendingIcons(
    const webapps::AppId& app_id,
    base::PassKey<ApplyPendingManifestUpdateCommand>,
    OverwriteAppIconsFromPendingIconsCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::OverwriteIconsFromPendingIcons");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!provider_->registrar_unsafe().IsInRegistrar(app_id)) {
    std::move(callback).Run(false);
    return;
  }
  const base::FilePath app_manifest_resources_dir =
      GetManifestResourcesDirectoryForApp(web_apps_directory_, app_id);
  const base::FilePath trusted_dir =
      app_manifest_resources_dir.Append(kTrustedIconFolderName);
  const base::FilePath pending_manifest_dir =
      app_manifest_resources_dir.Append(kPendingManifestIconFolderName);
  const base::FilePath pending_trusted_dir =
      app_manifest_resources_dir.Append(kPendingTrustedIconFolderName);

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OverwriteAppIconsFromPendingIconsBlocking,
                     app_manifest_resources_dir, trusted_dir,
                     pending_manifest_dir, pending_trusted_dir),
      base::BindOnce(&LogErrorsCallCallback<bool>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::DeletePendingIconData(
    const webapps::AppId& app_id,
    DeletePendingPassKey,
    DeletePendingIconDataCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::DeletePendingIconData");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!provider_->registrar_unsafe().IsInRegistrar(app_id)) {
    std::move(callback).Run(false);
    return;
  }

  const base::FilePath pending_manifest_dir =
      GetAppPendingManifestIconsDir(web_apps_directory_, app_id);
  const base::FilePath pending_trusted_dir =
      GetAppPendingTrustedIconsDir(web_apps_directory_, app_id);

  std::vector<base::FilePath> directories_to_delete;
  directories_to_delete.push_back(pending_manifest_dir);
  directories_to_delete.push_back(pending_trusted_dir);

  base::ConcurrentCallbacks<TypedResult<bool>> deletion_callbacks;

  for (const auto& directory : directories_to_delete) {
    icon_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&DeleteDirectoryAndGetResultBlocking, directory),
        deletion_callbacks.CreateCallback());
  }

  std::move(deletion_callbacks)
      .Done(base::BindOnce([](std::vector<TypedResult<bool>> results)
                               -> web_app::TypedResult<bool> {
              web_app::TypedResult<bool> final_result;
              for (const auto& result : results) {
                if (result.HasErrors()) {
                  final_result.error_log.push_back(result.error_log[0]);
                }
              }

              if (final_result.HasErrors()) {
                return final_result;
              }

              return {.value = true};
            })
                .Then(base::BindOnce(&LogErrorsCallCallback<bool>, GetWeakPtr(),
                                     std::move(callback))));
}

SkBitmap WebAppIconManager::GetFavicon(const webapps::AppId& app_id) const {
  auto iter = favicon_cache_.find(app_id);
  if (iter == favicon_cache_.end())
    return SkBitmap();

  const gfx::ImageSkia& image_skia = iter->second;

  // A representation for 1.0 UI scale factor is mandatory. GetRepresentation()
  // should create one.
  return image_skia.GetRepresentation(1.0f).GetBitmap();
}

gfx::ImageSkia WebAppIconManager::GetFaviconImageSkia(
    const webapps::AppId& app_id) const {
  auto iter = favicon_cache_.find(app_id);
  return iter != favicon_cache_.end() ? iter->second : gfx::ImageSkia();
}

gfx::ImageSkia WebAppIconManager::GetMonochromeFavicon(
    const webapps::AppId& app_id) const {
  auto iter = favicon_monochrome_cache_.find(app_id);
  return iter != favicon_monochrome_cache_.end() ? iter->second
                                                 : gfx::ImageSkia();
}

void WebAppIconManager::OnWebAppInstalled(const webapps::AppId& app_id) {
  TRACE_EVENT0("ui", "WebAppIconManager::OnWebAppInstalled");
  ReadFavicon(app_id);
  // Monochrome icons are used in tabbed apps and for ChromeOS notifications.
  ReadMonochromeFavicon(app_id);
}

void WebAppIconManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void WebAppIconManager::ReadIconAndResize(const webapps::AppId& app_id,
                                          IconPurpose purpose,
                                          SquareSizePx desired_icon_size,
                                          ReadIconsCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadIconAndResize");
  std::optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, {purpose}, desired_icon_size);
  if (!best_icon) {
    best_icon = FindIconMatchSmaller(app_id, {purpose}, desired_icon_size);
  }

  if (!best_icon) {
    std::move(callback).Run(SizeToBitmap());
    return;
  }

  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ReadIconAndResizeBlocking, provider_->file_utils(),
                     web_apps_directory_, std::move(icon_id), desired_icon_size,
                     best_icon->is_trusted),
      base::BindOnce(&LogErrorsCallCallback<SizeToBitmap>, GetWeakPtr(),
                     std::move(callback)));
}

void WebAppIconManager::ReadFavicons(const webapps::AppId& app_id,
                                     IconPurpose purpose,
                                     ReadImageSkiaCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadFavicons");
  SortedSizesPx ui_scale_factors_px_sizes;
  auto size_and_purpose =
      FindIconMatchBigger(app_id, {purpose}, gfx::kFaviconSize,
                          /*skip_trusted_icons_for_favicons=*/true);
  if (!size_and_purpose.has_value()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  ui_scale_factors_px_sizes.insert(size_and_purpose->size_px);

  for (const auto scale : {2, 3, 4}) {
    size_and_purpose =
        FindIconMatchSmaller(app_id, {purpose}, gfx::kFaviconSize * scale,
                             /*skip_trusted_icons_for_favicons=*/true);
    if (size_and_purpose.has_value()) {
      ui_scale_factors_px_sizes.insert(size_and_purpose->size_px);
    }
  }
  // If we didn't find any icons between 32-64px, look for a larger icon we can
  // downsize.
  if (*ui_scale_factors_px_sizes.rbegin() < 32) {
    size_and_purpose =
        FindIconMatchBigger(app_id, {purpose}, gfx::kFaviconSize * 4,
                            /*skip_trusted_icons_for_favicons=*/true);
    if (size_and_purpose.has_value()) {
      ui_scale_factors_px_sizes.insert(size_and_purpose->size_px);
    }
  }

  // Favicons aren't shown on security sensitive surfaces, and sometimes
  // monochrome icons are needed (like for the home tab favicon), which is not
  // available as part of the trusted icons storage. As such, reading from the
  // untrusted icons is fine here.
  ReadUntrustedIcons(app_id, purpose, ui_scale_factors_px_sizes,
                     base::BindOnce(&WebAppIconManager::OnReadFavicons,
                                    GetWeakPtr(), std::move(callback)));
}

void WebAppIconManager::OnReadFavicons(ReadImageSkiaCallback callback,
                                       IconMetadataFromDisk icon_metadata) {
  TRACE_EVENT0("ui", "WebAppIconManager::OnReadFavicons");
  std::move(callback).Run(
      ConvertFaviconBitmapsToImageSkia(std::move(icon_metadata.icons_map)));
}

void WebAppIconManager::CheckForEmptyOrMissingIconFiles(
    const webapps::AppId& app_id,
    base::OnceCallback<void(IconFilesCheck)> callback) const {
  TRACE_EVENT0("ui", "WebAppIconManager::CheckForEmptyOrMissingIconFiles");
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run({});
    return;
  }

  base::flat_map<IconPurpose, SortedSizesPx> manifest_icon_purpose_to_sizes;
  base::flat_map<IconPurpose, SortedSizesPx> trusted_icon_purpose_to_sizes;
  for (const IconPurpose& purpose : kIconPurposes) {
    manifest_icon_purpose_to_sizes[purpose] =
        web_app->downloaded_icon_sizes(purpose);
    if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon) &&
        purpose != IconPurpose::MONOCHROME) {
      trusted_icon_purpose_to_sizes[purpose] =
          web_app->stored_trusted_icon_sizes(purpose);
    }
  }

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(CheckForEmptyOrMissingIconFilesBlocking,
                     provider_->file_utils(), web_apps_directory_, app_id,
                     std::move(manifest_icon_purpose_to_sizes),
                     std::move(trusted_icon_purpose_to_sizes)),
      std::move(callback));
}

void WebAppIconManager::SetFaviconReadCallbackForTesting(
    FaviconReadCallback callback) {
  favicon_read_callback_ = std::move(callback);
}

void WebAppIconManager::SetFaviconMonochromeReadCallbackForTesting(
    FaviconReadCallback callback) {
  favicon_monochrome_read_callback_ = std::move(callback);
}

base::FilePath WebAppIconManager::GetIconFilePathForTesting(
    const webapps::AppId& app_id,
    IconPurpose purpose,
    SquareSizePx size) {
  std::optional<IconSizeAndPurpose> best_icon =
      FindIconMatchBigger(app_id, {purpose}, size);
  if (!best_icon) {
    best_icon = FindIconMatchSmaller(app_id, {purpose}, size);
  }

  if (!best_icon) {
    return base::FilePath();
  }

  IconId icon_id(app_id, best_icon->purpose, best_icon->size_px);
  if (best_icon->is_trusted) {
    return GetIconsFileNameForChildDirectory(
        web_apps_directory_, base::FilePath(kTrustedIconFolderName), icon_id);
  }

  return GetIconFileName(web_apps_directory_, icon_id);
}

base::FilePath WebAppIconManager::GetAppPendingTrustedIconDirForTesting(
    const webapps::AppId& app_id) {
  return GetAppPendingTrustedIconsDir(web_apps_directory_, app_id);
}

base::FilePath WebAppIconManager::GetAppPendingManifestIconDirForTesting(
    const webapps::AppId& app_id) {
  return GetAppPendingManifestIconsDir(web_apps_directory_, app_id);
}

base::WeakPtr<const WebAppIconManager> WebAppIconManager::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<WebAppIconManager> WebAppIconManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppIconManager::ReadUntrustedIcons(const webapps::AppId& app_id,
                                           IconPurpose purpose,
                                           const SortedSizesPx& icon_sizes,
                                           ReadIconMetadataCallback callback) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadIcons");
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!provider_->registrar_unsafe().GetAppById(app_id)) {
    std::move(callback).Run(IconMetadataFromDisk());
    return;
  }
  DCHECK(HasIcons(app_id, purpose, icon_sizes));

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          ReadIconsBlocking, provider_->file_utils(), web_apps_directory_,
          app_id, purpose,
          std::vector<SquareSizePx>(icon_sizes.begin(), icon_sizes.end()),
          /*read_trusted_icons=*/false),
      base::BindOnce(&LogErrorsCallCallback<IconMetadataFromDisk>, GetWeakPtr(),
                     std::move(callback)));
}

std::optional<WebAppIconManager::IconSizeAndPurpose>
WebAppIconManager::FindIconMatchSmaller(
    const webapps::AppId& app_id,
    const std::vector<IconPurpose>& purposes,
    SquareSizePx max_size,
    bool skip_trusted_icons_for_favicons) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app)
    return std::nullopt;

  if (base::FeatureList::IsEnabled(features::kWebAppUsePrimaryIcon) &&
      !skip_trusted_icons_for_favicons) {
    // Must iterate through purposes in order given.
    for (IconPurpose purpose : purposes) {
      if (purpose == IconPurpose::MONOCHROME) {
        continue;
      }
      // Must iterate sizes from smallest to largest.
      const SortedSizesPx& sizes = web_app->stored_trusted_icon_sizes(purpose);
      for (SquareSizePx size : base::Reversed(sizes)) {
        if (size <= max_size) {
          return IconSizeAndPurpose{size, purpose, /*is_trusted=*/true};
        }
      }
    }
  }

  // Must check purposes in the order given.
  for (IconPurpose purpose : purposes) {
    // Must iterate sizes from largest to smallest.
    const SortedSizesPx& sizes = web_app->downloaded_icon_sizes(purpose);
    for (SquareSizePx size : base::Reversed(sizes)) {
      if (size <= max_size)
        return IconSizeAndPurpose{size, purpose};
    }
  }

  return std::nullopt;
}

void WebAppIconManager::ReadFavicon(const webapps::AppId& app_id) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadFavicon");
  ReadFavicons(
      app_id, IconPurpose::ANY,
      base::BindOnce(&WebAppIconManager::OnReadFavicon, GetWeakPtr(), app_id));
}

void WebAppIconManager::OnReadFavicon(const webapps::AppId& app_id,
                                      gfx::ImageSkia image_skia) {
  TRACE_EVENT0("ui", "WebAppIconManager::OnReadFavicon");
  if (!image_skia.isNull())
    favicon_cache_[app_id] = image_skia;

  if (favicon_read_callback_)
    favicon_read_callback_.Run(app_id);
}

void WebAppIconManager::ReadMonochromeFavicon(const webapps::AppId& app_id) {
  TRACE_EVENT0("ui", "WebAppIconManager::ReadMonochromeFavicon");
  ReadFavicons(app_id, IconPurpose::MONOCHROME,
               base::BindOnce(&WebAppIconManager::OnReadMonochromeFavicon,
                              GetWeakPtr(), app_id));
}

void WebAppIconManager::OnReadMonochromeFavicon(
    const webapps::AppId& app_id,
    gfx::ImageSkia manifest_monochrome_image) {
  TRACE_EVENT0("ui", "WebAppIconManager::OnReadMonochromeFavicon");
  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app)
    return;

  if (manifest_monochrome_image.isNull()) {
    OnMonochromeIconConverted(app_id, manifest_monochrome_image);
    return;
  }

  const SkColor solid_color =
      web_app->theme_color() ? *web_app->theme_color() : SK_ColorDKGRAY;

  manifest_monochrome_image.MakeThreadSafe();

  icon_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(ConvertImageToSolidFillMonochrome, solid_color,
                     std::move(manifest_monochrome_image)),
      base::BindOnce(&WebAppIconManager::OnMonochromeIconConverted,
                     GetWeakPtr(), app_id));
}

void WebAppIconManager::OnMonochromeIconConverted(
    const webapps::AppId& app_id,
    gfx::ImageSkia converted_image) {
  TRACE_EVENT0("ui", "WebAppIconManager::OnMonochromeIconConverted");
  if (!converted_image.isNull())
    favicon_monochrome_cache_[app_id] = converted_image;

  if (favicon_monochrome_read_callback_)
    favicon_monochrome_read_callback_.Run(app_id);
}

}  // namespace web_app
