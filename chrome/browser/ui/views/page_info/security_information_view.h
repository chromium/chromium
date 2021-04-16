// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SECURITY_INFORMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SECURITY_INFORMATION_VIEW_H_

#include "components/page_info/page_info.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"

namespace views {
class StyledLabel;
}  // namespace views

// View that represents the header of the page info bubble. The header shows the
// status of the site's identity check and the name of the site's identity.
class SecurityInformationView : public views::View {
 public:
  METADATA_HEADER(SecurityInformationView);
  explicit SecurityInformationView(int side_margin);
  SecurityInformationView(const SecurityInformationView&) = delete;
  SecurityInformationView& operator=(const SecurityInformationView&) = delete;
  ~SecurityInformationView() override;

  // Sets the security summary for the current page.
  void SetSummary(const std::u16string& summary_text);

  // Sets the security details for the current page and the callback for the
  // "Learn more" link.
  void SetDetails(const std::u16string& details_text,
                  views::Link::ClickedCallback security_details_callback);

  // Adds the reset decision label and sets the callback for the link part of
  // the label.
  void AddResetDecisionsLabel(base::RepeatingClosure reset_decisions_callback);

  // Adds the change password and mark site as legitimate buttons and sets
  // passed callbacks to them. Based on |safe_browsing_status|, the label of
  // "change password" button will be chosen (Change password, Check password
  // or Protect account).
  void AddPasswordReuseButtons(
      PageInfo::SafeBrowsingStatus safe_browsing_status,
      views::Button::PressedCallback change_password_callback,
      views::Button::PressedCallback password_reuse_callback);

 private:
  // The label that displays the status of the identity check for this site.
  // Includes a link to open the Chrome Help Center article about connection
  // security.
  views::StyledLabel* security_details_label_ = nullptr;

  // A container for the styled label containing organization name and
  // jurisdiction details, if the site has an EV certificate.
  // This is only shown sometimes, so we use a container to keep track of where
  // to place it (if needed).
  views::View* ev_certificate_label_container_ = nullptr;

  // A container for the styled label with a link for resetting cert decisions.
  // This is only shown sometimes, so we use a container to keep track of
  // where to place it (if needed).
  views::View* reset_decisions_label_container_ = nullptr;

  // A container for the label buttons used to change password or mark the site
  // as safe.
  views::View* password_reuse_button_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_SECURITY_INFORMATION_VIEW_H_
