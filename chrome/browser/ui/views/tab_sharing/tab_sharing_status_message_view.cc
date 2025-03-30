// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_status_message_view.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace {
using TabRole = ::TabSharingInfoBarDelegate::TabRole;

constexpr auto kButtonInsets = gfx::Insets::VH(2, 8);

std::vector<std::u16string> EndpointInfosToStrings(
    const std::vector<TabSharingStatusMessageView::EndpointInfo>&
        endpoint_infos) {
  std::vector<std::u16string> res;
  for (const TabSharingStatusMessageView::EndpointInfo& endpoint_info :
       endpoint_infos) {
    res.push_back(endpoint_info.text);
  }
  return res;
}

void ActivateWebContents(content::GlobalRenderFrameHostId focus_target_id) {
  content::RenderFrameHost* const rfh =
      content::RenderFrameHost::FromID(focus_target_id);
  if (!rfh) {
    return;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents || !web_contents->GetDelegate()) {
    return;
  }

  web_contents->GetDelegate()->ActivateContents(web_contents);
}

TabSharingStatusMessageView::MessageInfo GetMessageInfoCastingNoSinkName(
    bool shared_tab,
    const TabSharingStatusMessageView::EndpointInfo& shared_tab_info) {
  if (shared_tab) {
    return TabSharingStatusMessageView::MessageInfo(
        IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_NO_DEVICE_NAME_LABEL,
        /*endpoint_infos=*/{});
  }
  return shared_tab_info.text.empty()
             ? TabSharingStatusMessageView::MessageInfo(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_NO_DEVICE_NAME_LABEL,
                   /*endpoint_infos=*/{})
             : TabSharingStatusMessageView::MessageInfo(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_NO_DEVICE_NAME_LABEL,
                   {shared_tab_info});
}

TabSharingStatusMessageView::MessageInfo GetMessageInfoCasting(
    bool shared_tab,
    const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
    const std::u16string& sink_name) {
  if (sink_name.empty()) {
    return GetMessageInfoCastingNoSinkName(shared_tab, shared_tab_info);
  }

  TabSharingStatusMessageView::EndpointInfo sink_info(
      sink_name, content::GlobalRenderFrameHostId());

  if (shared_tab) {
    return TabSharingStatusMessageView::MessageInfo(
        IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_LABEL, {sink_info});
  }
  return shared_tab_info.text.empty()
             ? TabSharingStatusMessageView::MessageInfo(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_LABEL,
                   {sink_info})
             : TabSharingStatusMessageView::MessageInfo(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_LABEL,
                   {shared_tab_info, sink_info});
}

TabSharingStatusMessageView::MessageInfo GetMessageInfoCapturing(
    bool shared_tab,
    const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
    const TabSharingStatusMessageView::EndpointInfo& capturer_info) {
  if (shared_tab) {
    return TabSharingStatusMessageView::MessageInfo(
        IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, {capturer_info});
  }
  return !shared_tab_info.text.empty()
             ? TabSharingStatusMessageView::MessageInfo(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                   {shared_tab_info, capturer_info})
             : TabSharingStatusMessageView::MessageInfo(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_UNTITLED_TAB_LABEL,
                   {capturer_info});
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

TabSharingStatusMessageView::MessageInfo GetMessageInfo(
    const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
    const TabSharingStatusMessageView::EndpointInfo& capturer_info,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  switch (capture_type) {
    case TabSharingInfoBarDelegate::TabShareType::CAST:
      return GetMessageInfoCasting(IsCapturedTab(role), shared_tab_info,
                                   capturer_name);

    case TabSharingInfoBarDelegate::TabShareType::CAPTURE:
      return GetMessageInfoCapturing(IsCapturedTab(role), shared_tab_info,
                                     capturer_info);
  }
  NOTREACHED();
}

}  // namespace

TabSharingStatusMessageView::EndpointInfo::EndpointInfo(
    std::u16string text,
    content::GlobalRenderFrameHostId focus_target_id)
    : text(std::move(text)), focus_target_id(focus_target_id) {}

TabSharingStatusMessageView::MessageInfo::MessageInfo(
    int message_id,
    std::vector<EndpointInfo> endpoint_infos)
    : MessageInfo(ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                      message_id),
                  std::move(endpoint_infos)) {}

TabSharingStatusMessageView::MessageInfo::MessageInfo(
    std::u16string format_string,
    std::vector<EndpointInfo> endpoint_infos)
    : format_string(std::move(format_string)),
      endpoint_infos(std::move(endpoint_infos)) {}

TabSharingStatusMessageView::MessageInfo::~MessageInfo() = default;

TabSharingStatusMessageView::MessageInfo::MessageInfo(
    const MessageInfo& other) = default;

TabSharingStatusMessageView::MessageInfo&
TabSharingStatusMessageView::MessageInfo::operator=(const MessageInfo& other) =
    default;

TabSharingStatusMessageView::MessageInfo::MessageInfo(MessageInfo&& other) =
    default;

TabSharingStatusMessageView::MessageInfo&
TabSharingStatusMessageView::MessageInfo::operator=(MessageInfo&& other) =
    default;

std::unique_ptr<views::View> TabSharingStatusMessageView::Create(
    content::GlobalRenderFrameHostId capturer_id,
    const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
    const TabSharingStatusMessageView::EndpointInfo& capturer_info,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  return std::make_unique<TabSharingStatusMessageView>(GetMessageInfo(
      shared_tab_info, capturer_info, capturer_name, role, capture_type));
}

std::u16string TabSharingStatusMessageView::GetMessageText(
    const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
    const TabSharingStatusMessageView::EndpointInfo& capturer_info,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  TabSharingStatusMessageView::MessageInfo info = GetMessageInfo(
      shared_tab_info, capturer_info, capturer_name, role, capture_type);
  return l10n_util::FormatString(
      info.format_string, EndpointInfosToStrings(info.endpoint_infos), nullptr);
}

TabSharingStatusMessageView::TabSharingStatusMessageView(
    const MessageInfo& info) {
  AddChildViews(info);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(0);
}

TabSharingStatusMessageView::~TabSharingStatusMessageView() = default;

void TabSharingStatusMessageView::AddChildViews(MessageInfo info) {
  // Format the message text with one-character replacements and retrieve the
  // offsets to where the replacements should go. (The replacement needs to be
  // non-empty for the reordering to work correctly in the next step.)
  // TODO(crbug.com/380903159): For EndpointInfos without
  // focus_target_id, pass the text here instead of adding buttons further down.
  std::vector<size_t> offsets;
  const std::u16string label_text = l10n_util::FormatString(
      info.format_string,
      std::vector<std::u16string>(info.endpoint_infos.size(), u" "), &offsets);

  // Some languages have the replacements in reverse order in the localization
  // string. Swap the offsets and the endpoint_infos if that is the case.
  CHECK_EQ(offsets.size(), info.endpoint_infos.size());
  CHECK_LE(offsets.size(), 2u);
  if (offsets.size() == 2 && offsets[0] >= offsets[1]) {
    std::swap(offsets[0], offsets[1]);
    std::swap(info.endpoint_infos[0], info.endpoint_infos[1]);
  }

  // For each endpoint_info (if any), add:
  // - a label for the text coming before the endpoint_info (if present)
  // - a button for the endpoint_info
  for (size_t i = 0; i < info.endpoint_infos.size(); i++) {
    // Add one to offset to account for the single-character replacement.
    const size_t start = i == 0 ? 0 : offsets[i - 1] + 1;
    const size_t length = offsets[i] - start;
    if (length > 0) {
      AddChildView(
          std::make_unique<views::Label>(label_text.substr(start, length)));
    }
    AddButton(info.endpoint_infos[i]);
  }

  // Add a label for the text after the last button, if any; otherwise, this
  // label covers the entire string.
  // Add one to offset to account for the single-character replacement.
  const size_t start = offsets.empty() ? 0 : offsets.back() + 1;
  const size_t length = label_text.size() - start;
  if (length > 0) {
    AddChildView(
        std::make_unique<views::Label>(label_text.substr(start, length)));
  }
}

void TabSharingStatusMessageView::AddButton(const EndpointInfo& endpoint_info) {
  views::MdTextButton* button =
      AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&ActivateWebContents,
                              endpoint_info.focus_target_id),
          endpoint_info.text, views::style::CONTEXT_LABEL));
  button->SetStyle(ui::ButtonStyle::kTonal);
  button->SetCustomPadding(kButtonInsets);
  button->SetTextColor(views::Button::ButtonState::STATE_NORMAL,
                       ui::kColorSysOnSurface);
  button->SetBgColorIdOverride(ui::kColorSysNeutralContainer);
  button->SetLabelStyle(views::style::STYLE_PRIMARY);
}

BEGIN_METADATA(TabSharingStatusMessageView)
END_METADATA
