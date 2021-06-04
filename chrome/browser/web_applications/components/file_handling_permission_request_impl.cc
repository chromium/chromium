// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handling_permission_request_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace permissions {

FileHandlingPermissionRequestImpl::FileHandlingPermissionRequestImpl(
    const GURL& request_origin,
    ContentSettingsType content_settings_type,
    bool has_gesture,
    content::WebContents* web_contents,
    PermissionDecidedCallback permission_decided_callback,
    base::OnceClosure delete_callback)
    : PermissionRequestImpl(request_origin,
                            content_settings_type,
                            has_gesture,
                            std::move(permission_decided_callback),
                            std::move(delete_callback)) {
  auto* browser = web_contents->GetBrowserContext();
  if (!browser) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(browser);
  DCHECK(profile);

  std::u16string extensions_list =
      web_app::GetFileTypeAssociationsHandledByWebAppsForDisplay(
          profile, request_origin);
  message_text_fragment_ = l10n_util::GetStringFUTF16(
      IDS_WEB_APP_FILE_HANDLING_PERMISSION_TEXT, extensions_list);
}

FileHandlingPermissionRequestImpl::~FileHandlingPermissionRequestImpl() =
    default;

std::u16string FileHandlingPermissionRequestImpl::GetMessageTextFragment()
    const {
  return message_text_fragment_;
}

}  // namespace permissions
