// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCURACY_TIPS_ACCURACY_SERVICE_H_
#define COMPONENTS_ACCURACY_TIPS_ACCURACY_SERVICE_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace accuracy_tips {

// Checks accuracy information on URLs for AccuracyTips.
// Handles rate-limiting and feature checks.
class AccuracyService : public KeyedService {
 public:
  explicit AccuracyService(std::unique_ptr<AccuracyTipUI> ui);
  ~AccuracyService() override;

  AccuracyService(const AccuracyService&) = delete;
  AccuracyService& operator=(const AccuracyService&) = delete;

  // Callback for accuracy check result.
  using AccuracyCheckCallback = base::OnceCallback<void(AccuracyTipStatus)>;

  // Returns the accuracy status for |url|.
  virtual void CheckAccuracyStatus(const GURL& url,
                                   AccuracyCheckCallback callback);

  // Shows an accuracy tip UI for web_contents after checking rate limits.
  virtual void MaybeShowAccuracyTip(content::WebContents* web_contents);

 private:
  void OnAccuracyTipClosed(AccuracyTipUI::Interaction interaction);

  std::unique_ptr<AccuracyTipUI> ui_;
  GURL sample_url_;

  base::WeakPtrFactory<AccuracyService> weak_factory_{this};
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_SERVICE_H_
