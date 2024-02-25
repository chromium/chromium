// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_LABEL_H_

#include <memory>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

// Label styled for use in AppInfo dialog so accessible users can step through
// and have each line read.
// TODO(dfried): merge functionality into views::Label.
class AppInfoLabel : public views::Label {
  METADATA_HEADER(AppInfoLabel, views::Label)

 public:
  explicit AppInfoLabel(const std::u16string& text);
  ~AppInfoLabel() override;

  // See documentation on views::Label::Label().
  AppInfoLabel(const std::u16string& text,
               int text_context,
               int text_style = views::style::STYLE_PRIMARY,
               gfx::DirectionalityMode directionality_mode =
                   gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_LABEL_H_
