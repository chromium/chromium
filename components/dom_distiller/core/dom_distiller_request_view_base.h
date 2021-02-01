// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_REQUEST_VIEW_BASE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_REQUEST_VIEW_BASE_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/task_tracker.h"

namespace dom_distiller {

// Handles receiving data asynchronously for a specific entry, and passing
// it along to the data callback for the data source. Lifetime matches that of
// the current main frame's page in the Viewer instance.
class DomDistillerRequestViewBase : public ViewRequestDelegate,
                                    public DistilledPagePrefs::Observer {
 public:
  explicit DomDistillerRequestViewBase(
      DistilledPagePrefs* distilled_page_prefs);
  ~DomDistillerRequestViewBase() override;

  // Flag this request as an error and send the error page template.
  void FlagAsErrorPage();
  // Get if this viewer is in an error state.
  bool IsErrorPage();

  // ViewRequestDelegate implementation:
  void OnArticleReady(const DistilledArticleProto* article_proto) override;

  void OnArticleUpdated(ArticleDistillationUpdate article_update) override;

  void TakeViewerHandle(std::unique_ptr<ViewerHandle> viewer_handle);

 protected:
  // DistilledPagePrefs::Observer implementation:
  void OnChangeTheme(mojom::Theme new_theme) override;
  void OnChangeFontFamily(mojom::FontFamily new_font_family) override;
  void OnChangeFontScaling(float scaling) override;

  // Sends JavaScript to the attached Viewer, buffering data if the viewer isn't
  // ready.
  virtual void SendJavaScript(const std::string& buffer) = 0;

  // Sends JavaScript common to normal and error pages.
  void SendCommonJavaScript();

  // The handle to the view request towards the DomDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  std::unique_ptr<ViewerHandle> viewer_handle_;

  // Number of pages of the distilled article content that have been rendered by
  // the viewer.
  int page_count_;

  // Interface for accessing preferences for distilled pages.
  DistilledPagePrefs* distilled_page_prefs_;

  // Flag to tell this observer that the web contents are in an error state.
  bool is_error_page_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_REQUEST_VIEW_BASE_H_
