// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "ui/views/view.h"

class SidePanelEntry final {
 public:
  // TODO(pbos): Add an icon ImageModel here.
  SidePanelEntry(std::u16string name,
                 base::RepeatingCallback<std::unique_ptr<views::View>()>
                     create_content_callback);
  SidePanelEntry(const SidePanelEntry&) = delete;
  SidePanelEntry& operator=(const SidePanelEntry&) = delete;
  ~SidePanelEntry();

  std::unique_ptr<views::View> CreateContent();

 private:
  const std::u16string name_;

  base::RepeatingCallback<std::unique_ptr<views::View>()>
      create_content_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_H_
