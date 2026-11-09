// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_REQUEST_VIEW_BASE_H_
#define COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_REQUEST_VIEW_BASE_H_

#include <memory>
#include <sstream>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/task_tracker.h"
#include "components/web_distiller/core/web_distiller_service.h"

namespace web_distiller {

// Handles receiving data asynchronously for a specific entry, and passing
// it along to the data callback for the data source. Lifetime matches that of
// the current main frame's page in the Viewer instance.
class WebDistillerRequestViewBase : public ViewRequestDelegate,
                                    public DistilledPagePrefs::Observer {
 public:
  explicit WebDistillerRequestViewBase(
      DistilledPagePrefs* distilled_page_prefs);
  ~WebDistillerRequestViewBase() override;

  // Flag this request as an error and send the error page template.
  void FlagAsErrorPage();
  // Get if this viewer is in an error state.
  bool IsErrorPage();

  // ViewRequestDelegate implementation:
  void OnArticleReady(const DistilledArticleProto* article_proto) override;

  void TakeViewerHandle(std::unique_ptr<ViewerHandle> viewer_handle);

 protected:
  // DistilledPagePrefs::Observer implementation:
  void OnChangeTheme(mojom::Theme new_theme,
                     ThemeSettingsUpdateSource source) override;
  void OnChangeFontFamily(mojom::FontFamily new_font_family) override;
  void OnChangeFontScaling(float scaling) override;
  void OnChangeLinksEnabled(bool enabled) override;

  // Sends JavaScript to the attached Viewer, buffering data if the viewer isn't
  // ready.
  virtual void SendJavaScript(const std::string& buffer) = 0;

  // Sends JavaScript common to normal and error pages.
  void SendCommonJavaScript();

  // The handle to the view request towards the WebDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  std::unique_ptr<ViewerHandle> viewer_handle_;

  // Interface for accessing preferences for distilled pages.
  raw_ptr<DistilledPagePrefs> distilled_page_prefs_;

  // Flag to tell this observer that the web contents are in an error state.
  bool is_error_page_;
};

}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_WEB_DISTILLER_REQUEST_VIEW_BASE_H_
