// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_

#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "ui/views/view.h"

namespace glic {
class GlicButton;
}

class TabGlicContainer : public views::View {
  METADATA_HEADER(TabGlicContainer, views::View)
 public:
  explicit TabGlicContainer(TabStripController* tab_strip_controller);
  TabGlicContainer(const TabGlicContainer&) = delete;
  TabGlicContainer& operator=(const TabGlicContainer&) = delete;
  ~TabGlicContainer() override;

#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicButton* GetGlicButton() { return glic_button_; }
#endif  // BUILDFLAG(ENABLE_GLIC)

 private:
#if BUILDFLAG(ENABLE_GLIC)
  raw_ptr<glic::GlicButton, DanglingUntriaged> glic_button_ = nullptr;
#endif  // BUILDFLAG(ENABLE_GLIC)
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GLIC_CONTAINER_H_
