// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
#define CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/service/cast_service.h"
#include "url/gurl.h"

namespace chromecast {

class CastWebService;
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

  // CastContentWindow::Delegate implementation:
  void OnWindowDestroyed() override;
  bool CanHandleGesture(GestureType gesture_type) override;
  bool ConsumeGesture(GestureType gesture_type) override;
  void OnVisibilityChange(VisibilityType visibility_type) override;
  std::string GetId() override;

 private:
  const std::unique_ptr<CastWebViewFactory> web_view_factory_;
  const std::unique_ptr<CastWebService> web_service_;
  CastWebView::Scoped cast_web_view_;
  GURL startup_url_;

  base::WeakPtrFactory<CastServiceSimple> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CastServiceSimple);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
