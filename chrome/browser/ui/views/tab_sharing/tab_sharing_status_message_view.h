// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_

#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
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
    EndpointInfo(std::u16string text,
                 content::GlobalRenderFrameHostId focus_target_id);

    std::u16string text;
    content::GlobalRenderFrameHostId focus_target_id;
  };

  struct MessageInfo final {
    MessageInfo(int message_id, std::vector<EndpointInfo> endpoint_infos);
    MessageInfo(std::u16string format_string,
                std::vector<EndpointInfo> endpoint_infos);
    ~MessageInfo();

    MessageInfo(const MessageInfo& other);
    MessageInfo& operator=(const MessageInfo& other);

    MessageInfo(MessageInfo&& other);
    MessageInfo& operator=(MessageInfo&& other);

    std::u16string format_string;
    std::vector<EndpointInfo> endpoint_infos;
  };

  static std::unique_ptr<views::View> Create(
      content::GlobalRenderFrameHostId capturer_id,
      const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
      const TabSharingStatusMessageView::EndpointInfo& capturer_info,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type);

  static std::u16string GetMessageText(
      const TabSharingStatusMessageView::EndpointInfo& shared_tab_info,
      const TabSharingStatusMessageView::EndpointInfo& capturer_info,
      const std::u16string& capturer_name,
      TabSharingInfoBarDelegate::TabRole role,
      TabSharingInfoBarDelegate::TabShareType capture_type);

  explicit TabSharingStatusMessageView(const MessageInfo& info);
  TabSharingStatusMessageView(const TabSharingStatusMessageView&) = delete;
  TabSharingStatusMessageView& operator=(const TabSharingStatusMessageView&) =
      delete;
  ~TabSharingStatusMessageView() override;

 private:
  void AddChildViews(MessageInfo info);
  void AddButton(const EndpointInfo& endpoint_info);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_STATUS_MESSAGE_VIEW_H_
