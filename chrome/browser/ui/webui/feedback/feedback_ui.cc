// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/feedback_resources.h"
#include "chrome/grit/feedback_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

content::WebUIDataSource* CreateFeedbackHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFeedbackHost);
  source->AddResourcePaths(
      base::make_span(kFeedbackResources, kFeedbackResourcesSize));
  source->AddResourcePath("", IDR_FEEDBACK_DEFAULT_HTML);
  return source;
}

FeedbackUI::FeedbackUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateFeedbackHTMLSource());
}

FeedbackUI::~FeedbackUI() = default;
