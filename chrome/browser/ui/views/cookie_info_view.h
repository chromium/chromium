// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COOKIE_INFO_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COOKIE_INFO_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
class GridLayout;
class Textfield;
}

namespace net {
class CanonicalCookie;
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView
//
//  Responsible for displaying a tabular grid of Cookie information.
class CookieInfoView : public views::ScrollView {
 public:
  CookieInfoView();
  ~CookieInfoView() override;

  // Update the display from the specified CookieNode.
  void SetCookie(const std::string& domain,
                 const net::CanonicalCookie& cookie_node);

  // Clears the cookie display to indicate that no or multiple cookies are
  // selected.
  void ClearCookieDisplay();

  // Enables or disables the cookie property text fields.
  void EnableCookieDisplay(bool enabled);

 protected:
  // views::View:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

 private:
  // Layout helper routines.
  views::Textfield* AddLabelRow(int layout_id,
                                views::GridLayout* layout,
                                int label_message_id);

  // Sets up the view layout.
  void Init();

  // Individual property labels
  views::Textfield* name_value_field_ = nullptr;
  views::Textfield* content_value_field_ = nullptr;
  views::Textfield* domain_value_field_ = nullptr;
  views::Textfield* path_value_field_ = nullptr;
  views::Textfield* send_for_value_field_ = nullptr;
  views::Textfield* created_value_field_ = nullptr;
  views::Textfield* expires_value_field_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CookieInfoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COOKIE_INFO_VIEW_H_
