// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/generate_visual_elements_manifest_work_item.h"

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/setup/setup_constants.h"

namespace installer {

namespace {

std::string GenerateVisualElementsManifest(const base::Version& version) {
  // A printf-style format string for generating the visual elements manifest.
  // Required arguments, in order, are thrice:
  //   - Relative path to the VisualElements directory.
  //   - Logo suffix for the channel.
  static constexpr char kManifestTemplate[] =
      "<Application xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'>\r\n"
      "  <VisualElements\r\n"
      "      ShowNameOnSquare150x150Logo='on'\r\n"
      "      Square150x150Logo='%s\\Logo%s.png'\r\n"
      "      Square70x70Logo='%s\\SmallLogo%s.png'\r\n"
      "      Square44x44Logo='%s\\SmallLogo%s.png'\r\n"
      "      ForegroundText='light'\r\n"
      "      BackgroundColor='#5F6368'/>\r\n"
      "</Application>\r\n";

  // Construct the relative path to the versioned VisualElements directory.
  std::string elements_dir =
      base::StrCat({version.GetString(), "\\", kVisualElements});

  // Fill the manifest with the desired values.
  const std::string logo_suffix =
      base::WideToUTF8(install_static::InstallDetails::Get().logo_suffix());
  return base::StringPrintf(kManifestTemplate, elements_dir.c_str(),
                            logo_suffix.c_str(), elements_dir.c_str(),
                            logo_suffix.c_str(), elements_dir.c_str(),
                            logo_suffix.c_str());
}

}  // namespace

bool GenerateVisualElementsManifestWorkItem::DoImpl() {
  const base::FilePath visual_elements_dir =
      target_dir_.AppendASCII(version_.GetString())
          .AppendASCII(kVisualElements);

// Assets are unconditionally required if there is a VisualElements directory.
#if DCHECK_IS_ON()
  DCHECK(base::PathExists(visual_elements_dir.Append(base::StrCat(
      {L"Logo", install_static::InstallDetails::Get().logo_suffix(),
       L".png"}))));
#endif

  // Generate the manifest.
  const std::string manifest(GenerateVisualElementsManifest(version_));

  // Write the manifest to `target_dir_`\chrome.VisualElementsManifest.xml.
  const base::FilePath manifest_path =
      target_dir_.Append(kVisualElementsManifest);
  if (base::File manifest_file(
          manifest_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_WIN_EXCLUSIVE_READ |
                             base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                             base::File::FLAG_WIN_SHARE_DELETE |
                             base::File::FLAG_WIN_SEQUENTIAL_SCAN |
                             base::File::FLAG_CAN_DELETE_ON_CLOSE);
      manifest_file.IsValid()) {
    if (manifest_file.WriteAtCurrentPosAndCheck(base::as_byte_span(manifest))) {
      VLOG(1) << "Generated " << manifest_path;
      succeeded_ = true;
    } else {
      PLOG(ERROR) << "Failed writing " << manifest_path;
      // Mark the file so that it's deleted when `manifest_file` closes it;
      // see FLAG_CAN_DELETE_ON_CLOSE above.
      manifest_file.DeleteOnClose(true);
    }
  } else {
    PLOG(ERROR) << "Failed creating " << manifest_path;
  }

  return succeeded_;
}

void GenerateVisualElementsManifestWorkItem::RollbackImpl() {
  if (succeeded_) {
    base::DeleteFile(target_dir_.Append(kVisualElementsManifest));
  }
}

}  // namespace installer
