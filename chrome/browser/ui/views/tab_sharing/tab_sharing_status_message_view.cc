// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_status_message_view.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace {
using TabRole = ::TabSharingInfoBarDelegate::TabRole;

std::u16string GetMessageTextCastingNoSinkName(
    bool shared_tab,
    const std::u16string& shared_tab_name) {
  if (shared_tab) {
    return l10n_util::GetStringUTF16(
        IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_NO_DEVICE_NAME_LABEL);
  }
  return shared_tab_name.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_NO_DEVICE_NAME_LABEL)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_NO_DEVICE_NAME_LABEL,
                   shared_tab_name);
}

std::u16string GetMessageTextCasting(bool shared_tab,
                                     const std::u16string& shared_tab_name,
                                     const std::u16string& sink_name) {
  if (sink_name.empty()) {
    return GetMessageTextCastingNoSinkName(shared_tab, shared_tab_name);
  }

  if (shared_tab) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_LABEL, sink_name);
  }
  return shared_tab_name.empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_LABEL,
                   sink_name)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_LABEL,
                   shared_tab_name, sink_name);
}

std::u16string GetMessageTextCapturing(bool shared_tab,
                                       const std::u16string& shared_tab_name,
                                       const std::u16string& app_name) {
  if (shared_tab) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, app_name);
  }
  return !shared_tab_name.empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                   shared_tab_name, app_name)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_UNTITLED_TAB_LABEL,
                   app_name);
}

bool IsCapturedTab(TabRole role) {
  switch (role) {
    case TabRole::kCapturingTab:
    case TabRole::kOtherTab:
      return false;
    case TabRole::kCapturedTab:
    case TabRole::kSelfCapturingTab:
      return true;
  }
  NOTREACHED();
}

std::u16string GetMessageText(
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  switch (capture_type) {
    case TabSharingInfoBarDelegate::TabShareType::CAST:
      return GetMessageTextCasting(IsCapturedTab(role), shared_tab_name,
                                   capturer_name);

    case TabSharingInfoBarDelegate::TabShareType::CAPTURE:
      return GetMessageTextCapturing(IsCapturedTab(role), shared_tab_name,
                                     capturer_name);
  }
  NOTREACHED();
}

}  // namespace

std::u16string TabSharingStatusMessageView::GetMessageText(
    const std::u16string& shared_tab_name,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  return ::GetMessageText(shared_tab_name, capturer_name, role, capture_type);
}

BEGIN_METADATA(TabSharingStatusMessageView)
END_METADATA
