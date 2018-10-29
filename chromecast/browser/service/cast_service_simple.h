// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
#define CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_

#include <memory>

#include "base/macros.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/service/cast_service.h"
#include "url/gurl.h"

namespace chromecast {

class CastWebContentsManager;
class CastWebViewFactory;
class CastWindowManager;

namespace shell {

class CastServiceSimple : public CastService, public CastWebView::Delegate {
 public:
  CastServiceSimple(content::BrowserContext* browser_context,
                    PrefService* pref_service,
                    CastWindowManager* window_manager);
  ~CastServiceSimple() override;

 protected:
  // CastService implementation:
  void InitializeInternal() override;
  void FinalizeInternal() override;
  void StartInternal() override;
  void StopInternal() override;

  // CastWebView::Delegate implementation:
  void OnPageStopped(CastWebContents* cast_web_contents,
                     int error_code) override;
  void OnPageStateChanged(CastWebContents* cast_web_contents) override;
  bool OnAddMessageToConsoleReceived(content::WebContents* source,
                                     int32_t level,
                                     const base::string16& message,
                                     int32_t line_no,
                                     const base::string16& source_id) override;

  // CastContentWindow::Delegate implementation:
  void OnWindowDestroyed() override;
  void OnKeyEvent(const ui::KeyEvent& key_event) override;
  bool CanHandleGesture(GestureType gesture_type) override;
  bool ConsumeGesture(GestureType gesture_type) override;
  void OnVisibilityChange(VisibilityType visibility_type) override;
  std::string GetId() override;

 private:
  CastWindowManager* const window_manager_;
  const std::unique_ptr<CastWebViewFactory> web_view_factory_;
  const std::unique_ptr<CastWebContentsManager> web_contents_manager_;
  std::unique_ptr<CastWebView> cast_web_view_;
  GURL startup_url_;

  DISALLOW_COPY_AND_ASSIGN(CastServiceSimple);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
