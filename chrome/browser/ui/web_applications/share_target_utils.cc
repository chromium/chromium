// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/share_target_utils.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_share_target/target_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "extensions/common/constants.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/profiles/profile.h"
#endif

namespace web_app {

bool SharedField::operator==(const SharedField& other) const {
  return name == other.name && value == other.value;
}

std::vector<SharedField> ExtractSharedFields(
    const apps::ShareTarget& share_target,
    const apps::Intent& intent) {
  std::vector<SharedField> result;

  if (!share_target.params.title.empty() && intent.share_title.has_value() &&
      !intent.share_title->empty()) {
    result.push_back(
        {.name = share_target.params.title, .value = *intent.share_title});
  }

  if (!intent.share_text.has_value())
    return result;

  apps_util::SharedText extracted_text =
      apps_util::ExtractSharedText(*intent.share_text);

  if (!share_target.params.text.empty() && !extracted_text.text.empty())
    result.push_back(
        {.name = share_target.params.text, .value = extracted_text.text});

  if (!share_target.params.url.empty() && !extracted_text.url.is_empty())
    result.push_back(
        {.name = share_target.params.url, .value = extracted_text.url.spec()});

  return result;
}

NavigateParams NavigateParamsForShareTarget(
    Browser* browser,
    const apps::ShareTarget& share_target,
    const apps::Intent& intent,
    const std::vector<base::FilePath>& launch_files) {
  NavigateParams nav_params(browser, share_target.action,
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

#if BUILDFLAG(IS_CHROMEOS)
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<bool> is_value_file_uris;
  std::vector<std::string> filenames;
  std::vector<std::string> types;

  if (intent.mime_type.has_value() && !intent.files.empty()) {
    if (!launch_files.empty()) {
      DCHECK_EQ(launch_files.size(), intent.files.size());
    }

    // Files for Web Share intents are created by the browser in
    // a .WebShare directory, with generated file names and file urls - see
    // //chrome/browser/webshare/chromeos/sharesheet_client.cc
    for (size_t i = 0; i < intent.files.size(); ++i) {
      const apps::IntentFilePtr& file = intent.files[i];

      const std::string& mime_type = file->mime_type.has_value()
                                         ? file->mime_type.value()
                                         : intent.mime_type.value();
      std::string name;
      for (const apps::ShareTarget::Files& files : share_target.params.files) {
        // Filter on MIME types. Chrome OS does not filter on file extensions.
        // https://w3c.github.io/web-share-target/level-2/#dfn-accepted
        if (base::ranges::any_of(
                files.accept, [&mime_type](const auto& criteria) {
                  return !base::StartsWith(criteria, ".") &&
                         net::MatchesMimeType(criteria, mime_type);
                })) {
          name = files.name;
          break;
        }
      }
      if (name.empty())
        continue;

      storage::FileSystemURL file_system_url;

#if BUILDFLAG(IS_CHROMEOS_ASH)
      storage::FileSystemContext* file_system_context =
          file_manager::util::GetFileManagerFileSystemContext(
              browser->profile());
      file_system_url =
          file_system_context->CrackURLInFirstPartyContext(file->url);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

      const std::string filename =
          (file->file_name.has_value() && !file->file_name->path().empty())
              ? (file->file_name->path().AsUTF8Unsafe())
              : file_system_url.path().BaseName().AsUTF8Unsafe();

      names.push_back(name);

      if (launch_files.empty()) {
        if (file->url.SchemeIsFile()) {
          base::FilePath file_path;
          net::FileURLToFilePath(file->url, &file_path);
          values.push_back(file_path.value());
        } else {
          values.push_back(file_system_url.path().AsUTF8Unsafe());
        }
      } else {
        values.push_back(launch_files[i].value());
      }

      is_value_file_uris.push_back(true);
      filenames.push_back(filename);
      types.push_back(mime_type);
    }
  }

  std::vector<SharedField> shared_fields =
      ExtractSharedFields(share_target, intent);
  for (const auto& shared_field : shared_fields) {
    DCHECK(!shared_field.value.empty());
    names.push_back(shared_field.name);
    values.push_back(shared_field.value);
    is_value_file_uris.push_back(false);
    filenames.emplace_back();
    types.push_back("text/plain");
  }

  if (share_target.enctype == apps::ShareTarget::Enctype::kMultipartFormData) {
    const std::string boundary = net::GenerateMimeMultipartBoundary();
    nav_params.extra_headers = base::StringPrintf(
        "Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
    nav_params.post_data = web_share_target::ComputeMultipartBody(
        names, values, is_value_file_uris, filenames, types, boundary);
  } else {
    const std::string serialization =
        web_share_target::ComputeUrlEncodedBody(names, values);
    if (share_target.method == apps::ShareTarget::Method::kPost) {
      nav_params.extra_headers =
          "Content-Type: application/x-www-form-urlencoded\r\n";
      nav_params.post_data = network::ResourceRequestBody::CreateFromBytes(
          serialization.c_str(), serialization.length());
    } else {
      GURL::Replacements replacements;
      replacements.SetQueryStr(serialization);
      nav_params.url = nav_params.url.ReplaceComponents(replacements);
    }
  }
#else
  // TODO(crbug.com/40158988): Support Web Share Target on Windows.
  // TODO(crbug.com/40734106): Support Web Share Target on Mac.
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_CHROMEOS)

  return nav_params;
}

}  // namespace web_app
