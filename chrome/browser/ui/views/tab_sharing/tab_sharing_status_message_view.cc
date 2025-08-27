// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_status_message_view.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/capture_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
using EndpointInfo = ::TabSharingStatusMessageView::EndpointInfo;
using InteractionWithControls = ::GetDisplayMediaUserInteractionWithControls;
using MessageInfo = ::TabSharingStatusMessageView::MessageInfo;
using TabRole = ::TabSharingInfoBarDelegate::TabRole;

constexpr auto kButtonInsets = gfx::Insets::VH(2, 8);
constexpr auto kSeparatorInsets = gfx::Insets::TLBR(0, 16, 0, 0);

std::vector<std::u16string> EndpointInfosToStrings(
    const std::vector<EndpointInfo>& endpoint_infos) {
  std::vector<std::u16string> res;
  for (const EndpointInfo& endpoint_info : endpoint_infos) {
    res.push_back(endpoint_info.text);
  }
  return res;
}

InteractionWithControls GetTabSharingInfoBarInteraction(
    TabRole tab_role_for_uma,
    EndpointInfo::TargetType target_type) {
  switch (tab_role_for_uma) {
    case TabRole::kCapturingTab:
      CHECK_EQ(target_type, EndpointInfo::TargetType::kCapturedTab);
      return InteractionWithControls::kCapturingToCapturedClicked;
    case TabRole::kCapturedTab:
      CHECK_EQ(target_type, EndpointInfo::TargetType::kCapturingTab);
      return InteractionWithControls::kCapturedToCapturingClicked;
    case TabRole::kSelfCapturingTab:
      NOTREACHED();
    case TabRole::kOtherTab:
      switch (target_type) {
        case EndpointInfo::TargetType::kCapturedTab:
          return InteractionWithControls::kOtherToCapturedClicked;
        case EndpointInfo::TargetType::kCapturingTab:
          return InteractionWithControls::kOtherToCapturingClicked;
      }
  }
  NOTREACHED();
}

void ActivateWebContents(
    content::GlobalRenderFrameHostId focus_target_id,
    EndpointInfo::TargetType target_type,
    std::optional<TabSharingInfoBarDelegate::TabRole> tab_role_for_uma,
    base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger) {
  if (tab_role_for_uma) {
    if (uma_logger) {
      uma_logger->Log(
          GetTabSharingInfoBarInteraction(*tab_role_for_uma, target_type));
    }
  }

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

MessageInfo GetMessageInfoCastingNoSinkName(
    TabRole role,
    const EndpointInfo& shared_tab_info) {
  if (TabSharingInfoBarDelegate::IsCapturedTab(role)) {
    return MessageInfo(
        IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_NO_DEVICE_NAME_LABEL,
        /*endpoint_infos=*/{}, role);
  }
  return shared_tab_info.text.empty()
             ? MessageInfo(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_NO_DEVICE_NAME_LABEL,
                   /*endpoint_infos=*/{}, role)
             : MessageInfo(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_NO_DEVICE_NAME_LABEL,
                   {shared_tab_info}, role);
}

MessageInfo GetMessageInfoCasting(TabRole role,
                                  const EndpointInfo& shared_tab_info,
                                  const std::u16string& sink_name) {
  if (sink_name.empty()) {
    return GetMessageInfoCastingNoSinkName(role, shared_tab_info);
  }

  EndpointInfo sink_info(sink_name, EndpointInfo::TargetType::kCapturingTab,
                         content::GlobalRenderFrameHostId());

  if (TabSharingInfoBarDelegate::IsCapturedTab(role)) {
    return MessageInfo(IDS_TAB_CASTING_INFOBAR_CASTING_CURRENT_TAB_LABEL,
                       {sink_info}, role);
  }
  return shared_tab_info.text.empty()
             ? MessageInfo(
                   IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_UNTITLED_TAB_LABEL,
                   {sink_info}, role)
             : MessageInfo(IDS_TAB_CASTING_INFOBAR_CASTING_ANOTHER_TAB_LABEL,
                           {shared_tab_info, sink_info}, role);
}

MessageInfo GetMessageInfoCapturing(TabRole role,
                                    const EndpointInfo& shared_tab_info,
                                    const EndpointInfo& capturer_info) {
  if (role == TabRole::kSelfCapturingTab) {
    return MessageInfo(IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL,
                       {EndpointInfo(capturer_info.text,
                                     EndpointInfo::TargetType::kCapturingTab)},
                       role);
  }

  if (TabSharingInfoBarDelegate::IsCapturedTab(role)) {
    return MessageInfo(IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL,
                       {capturer_info}, role);
  }

  if (shared_tab_info.text.empty()) {
    return MessageInfo(
        IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_UNTITLED_TAB_LABEL,
        {capturer_info}, role);
  }

  if (base::FeatureList::IsEnabled(features::kTabCaptureInfobarLinks) &&
      TabSharingInfoBarDelegate::IsCapturingTab(role)) {
    return MessageInfo(
        IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_TO_THIS_TAB_LABEL,
        {shared_tab_info}, role);
  }

  return MessageInfo(IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                     {shared_tab_info, capturer_info}, role);
}

MessageInfo GetMessageInfo(
    const EndpointInfo& shared_tab_info,
    const EndpointInfo& capturer_info,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  switch (capture_type) {
    case TabSharingInfoBarDelegate::TabShareType::CAST:
      return GetMessageInfoCasting(role, shared_tab_info, capturer_name);

    case TabSharingInfoBarDelegate::TabShareType::CAPTURE:
      return GetMessageInfoCapturing(role, shared_tab_info, capturer_info);
  }
  NOTREACHED();
}

}  // namespace

EndpointInfo::EndpointInfo(std::u16string text,
                           TargetType target_type,
                           content::GlobalRenderFrameHostId focus_target_id)
    : text(std::move(text)),
      target_type(target_type),
      focus_target_id(focus_target_id) {}

MessageInfo::MessageInfo(
    int message_id,
    std::vector<EndpointInfo> endpoint_infos,
    std::optional<TabSharingInfoBarDelegate::TabRole> tab_role_for_uma)
    : MessageInfo(ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                      message_id),
                  std::move(endpoint_infos),
                  tab_role_for_uma) {}

MessageInfo::MessageInfo(
    std::u16string format_string,
    std::vector<EndpointInfo> endpoint_infos,
    std::optional<TabSharingInfoBarDelegate::TabRole> tab_role_for_uma)
    : format_string(std::move(format_string)),
      endpoint_infos(std::move(endpoint_infos)),
      tab_role_for_uma(tab_role_for_uma) {}

MessageInfo::~MessageInfo() = default;

MessageInfo::MessageInfo(const MessageInfo& other) = default;

MessageInfo& MessageInfo::operator=(const MessageInfo& other) = default;

MessageInfo::MessageInfo(MessageInfo&& other) = default;

MessageInfo& MessageInfo::operator=(MessageInfo&& other) = default;

std::unique_ptr<views::View> TabSharingStatusMessageView::Create(
    content::GlobalRenderFrameHostId capturer_id,
    const EndpointInfo& shared_tab_info,
    const EndpointInfo& capturer_info,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type,
    base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger) {
  return std::make_unique<TabSharingStatusMessageView>(
      GetMessageInfo(shared_tab_info, capturer_info, capturer_name, role,
                     capture_type),
      uma_logger);
}

std::u16string TabSharingStatusMessageView::GetMessageText(
    const EndpointInfo& shared_tab_info,
    const EndpointInfo& capturer_info,
    const std::u16string& capturer_name,
    TabSharingInfoBarDelegate::TabRole role,
    TabSharingInfoBarDelegate::TabShareType capture_type) {
  MessageInfo info = GetMessageInfo(shared_tab_info, capturer_info,
                                    capturer_name, role, capture_type);
  return l10n_util::FormatString(
      info.format_string, EndpointInfosToStrings(info.endpoint_infos), nullptr);
}

TabSharingStatusMessageView::TabSharingStatusMessageView(
    const MessageInfo& info,
    base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger)
    : uma_logger_(uma_logger) {
  SetupMessage(info);
  AddChildView(views::Builder<views::Separator>()
                   .SetProperty(views::kMarginsKey, kSeparatorInsets)
                   .SetProperty(views::kFlexBehaviorKey,
                                views::FlexSpecification(
                                    views::MinimumFlexSizeRule::kPreferred))
                   .Build());
  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
}

TabSharingStatusMessageView::~TabSharingStatusMessageView() = default;

gfx::Size TabSharingStatusMessageView::GetMinimumSize() const {
  return gfx::Size();
}

void TabSharingStatusMessageView::SetupMessage(MessageInfo info) {
  // Format the message text and retrieve the offsets to where the replacements
  // should go.
  std::vector<size_t> offsets;
  std::vector<std::u16string> replacements;
  for (const EndpointInfo& endpoint_info : info.endpoint_infos) {
    replacements.emplace_back(endpoint_info.text);
  }
  const std::u16string label_text =
      l10n_util::FormatString(info.format_string, replacements, &offsets);

  // Some languages have the replacements in reverse order in the localization
  // string. Swap the offsets and the endpoint_infos if that is the case.
  CHECK_EQ(offsets.size(), info.endpoint_infos.size());
  CHECK_LE(offsets.size(), 2u);
  if (offsets.size() == 2 && offsets[0] >= offsets[1]) {
    std::swap(offsets[0], offsets[1]);
    std::swap(replacements[0], replacements[1]);
    std::swap(info.endpoint_infos[0], info.endpoint_infos[1]);
  }

  // For each endpoint_info with a focus_target_id (if any):
  // - add a label for any plain text coming before the endpoint_info.
  // - add a button for the endpoint_info.
  // - update label_start to the end of the corresponding replacement.
  //
  // For endpoint_infos without focus_target_id (if any):
  // - no label or button is added.
  // - label_start is left unchanged.
  // This results in the text before the endpoint_info and the replacement text
  // being added to the next label.
  size_t label_start = 0;
  int flex_layout_order = 1;
  for (size_t i = 0; i < info.endpoint_infos.size(); ++i) {
    if (!info.endpoint_infos[i].focus_target_id) {
      continue;
    }
    if (label_start < offsets[i]) {
      const size_t label_length = offsets[i] - label_start;
      AddLabel(label_text.substr(label_start, label_length),
               flex_layout_order++);
    }
    AddButton(info.endpoint_infos[i], flex_layout_order++,
              info.tab_role_for_uma);
    label_start = offsets[i] + replacements[i].size();
  }

  // Add a label for the text after the last button, if any; otherwise, this
  // label covers the entire string.
  if (label_start < label_text.size()) {
    const size_t label_length = label_text.size() - label_start;
    AddLabel(label_text.substr(label_start, label_length), flex_layout_order++);
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  SetAccessibleName(label_text);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

void TabSharingStatusMessageView::AddLabel(const std::u16string& text,
                                           int flex_layout_order) {
  views::Label* label = AddChildView(std::make_unique<views::Label>(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT));
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero)
          .WithOrder(flex_layout_order));
}

void TabSharingStatusMessageView::AddButton(
    const EndpointInfo& endpoint_info,
    int flex_layout_order,
    std::optional<TabSharingInfoBarDelegate::TabRole> tab_role_for_uma) {
  views::MdTextButton* button =
      AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &ActivateWebContents, endpoint_info.focus_target_id,
              endpoint_info.target_type, tab_role_for_uma, uma_logger_),
          endpoint_info.text, views::style::CONTEXT_LABEL));
  button->SetStyle(ui::ButtonStyle::kTonal);
  button->SetCustomPadding(kButtonInsets);
  button->SetTextColor(views::Button::ButtonState::STATE_NORMAL,
                       ui::kColorLinkForeground);
  button->SetTextColor(views::Button::ButtonState::STATE_HOVERED,
                       ui::kColorLinkForeground);
  button->SetTextColor(views::Button::ButtonState::STATE_PRESSED,
                       ui::kColorLinkForeground);
  button->SetBgColorIdOverride(ui::kColorSysNeutralContainer);
  button->SetLabelStyle(views::style::STYLE_BODY_5_MEDIUM);
  button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero)
          .WithOrder(flex_layout_order));
}

BEGIN_METADATA(TabSharingStatusMessageView)
END_METADATA
