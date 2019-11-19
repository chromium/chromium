// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PANEL_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/view.h"

class Profile;

namespace extensions {
class Extension;
}
namespace views {
class Label;
}

// A piece of the App Info dialog that displays information for a particular
// profile and app. Panels in the App Info dialog extend this class.
class AppInfoPanel : public views::View {
 public:
  AppInfoPanel(Profile* profile, const extensions::Extension* app);

  ~AppInfoPanel() override;

 protected:
  // Closes the dialog.
  void Close();

  // Opens the given URL in a new browser tab.
  void OpenLink(const GURL& url);

  // Create a heading label with the given text.
  std::unique_ptr<views::Label> CreateHeading(const base::string16& text) const;

  // Create a view with a vertically-stacked box layout, which can have child
  // views appended to it. |child_spacing| defaults to
  // |views::kRelatedControlVerticalSpacing|.
  std::unique_ptr<views::View> CreateVerticalStack(int child_spacing) const;
  std::unique_ptr<views::View> CreateVerticalStack() const;

  // Create a view with a horizontally-stacked box layout, which can have child
  // views appended to it.
  std::unique_ptr<views::View> CreateHorizontalStack(int child_spacing) const;

  // Given a key and a value, displays them side-by-side as a field and its
  // value.
  // TODO(dfried): for ease of navigation, use GetStringFUTF16() and format the
  // key and value together, eliminating this method.
  std::unique_ptr<views::View> CreateKeyValueField(
      std::unique_ptr<views::View> key,
      std::unique_ptr<views::View> value) const;

  Profile* profile_;
  const extensions::Extension* app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInfoPanel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PANEL_H_
