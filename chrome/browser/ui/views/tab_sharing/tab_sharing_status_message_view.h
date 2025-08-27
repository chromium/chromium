// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_

#include <variant>

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/views/screen_sharing_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

// View representing the TabSharingInfoBar status message.
class TabSharingStatusMessageView : public views::View {
  METADATA_HEADER(TabSharingStatusMessageView, views::View)

 public:
  using ChildView =
      std::variant<raw_ptr<views::Label>, raw_ptr<views::MdTextButton>>;

  struct EndpointInfo final {
    enum class TargetType {
      kCapturedTab,
      kCapturingTab,
    };
    explicit EndpointInfo(std::u16string text,
                          TargetType target_type,
                          content::GlobalRenderFrameHostId focus_target_id =
                              content::GlobalRenderFrameHostId());

    std::u16string text;
    TargetType target_type;
    content::GlobalRenderFrameHostId focus_target_id;
  };

  struct MessageInfo final {
    MessageInfo(int message_id,
                std::vector<EndpointInfo> endpoint_infos,
                std::optional<TabSharingInfoBarDelegate::TabRole>
                    tab_role_for_uma = std::nullopt);
    MessageInfo(std::u16string format_string,
                std::vector<EndpointInfo> endpoint_infos,
                std::optional<TabSharingInfoBarDelegate::TabRole>
                    tab_role_for_uma = std::nullopt);
    ~MessageInfo();

    MessageInfo(const MessageInfo& other);
    MessageInfo& operator=(const MessageInfo& other);

    MessageInfo(MessageInfo&& other);
    MessageInfo& operator=(MessageInfo&& other);

    std::u16string format_string;
    std::vector<EndpointInfo> endpoint_infos;
    std::optional<TabSharingInfoBarDelegate::TabRole> tab_role_for_uma;
  };

  static std::unique_ptr<views::View> Create(
      content::GlobalRenderFrameHostId capturer_id,
      const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
      const TabSharingStatusMessageView::EndpointInfo& capturer_info,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type,
      base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger);

  static std::u16string GetMessageText(
      const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
      const TabSharingStatusMessageView::EndpointInfo& capturer_info,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type);

  TabSharingStatusMessageView(
      const MessageInfo& info,
      base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger);
  TabSharingStatusMessageView(const TabSharingStatusMessageView&) = delete;
  TabSharingStatusMessageView& operator=(const TabSharingStatusMessageView&) =
      delete;
  ~TabSharingStatusMessageView() override;

  // View:
  gfx::Size GetMinimumSize() const override;

 private:
  void SetupMessage(MessageInfo info);
  void AddLabel(const std::u16string& text, int flex_layout_order);
  void AddButton(
      const EndpointInfo& endpoint_info,
      int flex_layout_order,
      std::optional<TabSharingInfoBarDelegate::TabRole> tab_role_for_uma);

  const base::WeakPtr<ScreensharingControlsHistogramLogger> uma_logger_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_
