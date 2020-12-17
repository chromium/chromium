// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/share_target_utils.h"

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_share_target/target_util.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "extensions/common/constants.h"
#include "net/base/mime_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#endif

namespace web_app {

bool SharedField::operator==(const SharedField& other) const {
  return name == other.name && value == other.value;
}

std::vector<SharedField> ExtractSharedFields(
    const apps::ShareTarget& share_target,
    const apps::mojom::Intent& intent) {
  std::vector<SharedField> result;

  if (!share_target.params.title.empty() && intent.share_title.has_value() &&
      !intent.share_title->empty()) {
    result.push_back(
        {.name = share_target.params.title, .value = *intent.share_title});
  }

  if (!intent.share_text.has_value())
    return result;

  std::string extracted_text = *intent.share_text;
  GURL extracted_url;
  size_t last_space = extracted_text.find_last_of(' ');
  if (last_space == std::string::npos) {
    extracted_url = GURL(extracted_text);
    if (extracted_url.is_valid())
      extracted_text.clear();
  } else {
    extracted_url = GURL(extracted_text.substr(last_space + 1));
    if (extracted_url.is_valid())
      extracted_text.erase(last_space);
  }

  if (!share_target.params.text.empty() && !extracted_text.empty())
    result.push_back(
        {.name = share_target.params.text, .value = extracted_text});

  if (!share_target.params.url.empty() && extracted_url.is_valid())
    result.push_back(
        {.name = share_target.params.url, .value = extracted_url.spec()});

  return result;
}

NavigateParams NavigateParamsForShareTarget(
    Browser* browser,
    const apps::ShareTarget& share_target,
    const apps::mojom::Intent& intent) {
  NavigateParams nav_params(browser, share_target.action,
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<std::string> filenames;
  std::vector<std::string> types;
  std::vector<bool> is_value_file_uris;

  if (intent.mime_type.has_value() && intent.file_urls.has_value()) {
    const std::string& mime_type = intent.mime_type.value();
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
    if (!name.empty()) {
      // Files for Web Share intents are created by the browser in
      // a .WebShare directory, with generated file names and file urls - see
      // //chrome/browser/webshare/chromeos/sharesheet_client.cc
      for (const GURL& file_url : intent.file_urls.value()) {
        storage::FileSystemContext* file_system_context =
            file_manager::util::GetFileSystemContextForExtensionId(
                browser->profile(), extension_misc::kFilesManagerAppId);
        storage::FileSystemURL file_system_url =
            file_system_context->CrackURL(file_url);
        if (!file_system_url.is_valid()) {
          VLOG(1) << "Received unexpected file URL: " << file_url.spec();
          continue;
        }

        names.push_back(name);
        values.push_back(file_system_url.path().AsUTF8Unsafe());
        filenames.push_back(file_system_url.path().BaseName().AsUTF8Unsafe());
        types.push_back(mime_type);
        is_value_file_uris.push_back(true);
      }
    }
  }

  std::vector<SharedField> shared_fields =
      ExtractSharedFields(share_target, intent);
  for (const auto& shared_field : shared_fields) {
    DCHECK(!shared_field.value.empty());
    names.push_back(shared_field.name);
    values.push_back(shared_field.value);
    filenames.push_back(std::string());
    types.push_back("text/plain");
    is_value_file_uris.push_back(false);
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
      url::Replacements<char> replacements;
      replacements.SetQuery(serialization.c_str(),
                            url::Component(0, serialization.length()));
      nav_params.url = nav_params.url.ReplaceComponents(replacements);
    }
  }
#else
  // TODO(crbug.com/1153194): Support Web Share Target on Windows.
  // TODO(crbug.com/1153195): Support Web Share Target on Mac.
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return nav_params;
}

}  // namespace web_app
