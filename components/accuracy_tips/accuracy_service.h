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

namespace base {
class TimeTicks;
}

namespace content {
class WebContents;
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

namespace accuracy_tips {

class AccuracyTipSafeBrowsingClient;

// Checks accuracy information on URLs for AccuracyTips.
// Handles rate-limiting and feature checks.
class AccuracyService : public KeyedService {
 public:
  AccuracyService(
      std::unique_ptr<AccuracyTipUI> ui,
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~AccuracyService() override;

  AccuracyService(const AccuracyService&) = delete;
  AccuracyService& operator=(const AccuracyService&) = delete;

  // Callback for accuracy check result.
  using AccuracyCheckCallback = base::OnceCallback<void(AccuracyTipStatus)>;

  // Returns the accuracy status for |url|. Virtual for testing purposes.
  virtual void CheckAccuracyStatus(const GURL& url,
                                   AccuracyCheckCallback callback);

  // Shows an accuracy tip UI for web_contents after checking rate limits.
  // Virtual for testing purposes.
  virtual void MaybeShowAccuracyTip(content::WebContents* web_contents);

  void SetSampleUrlForTesting(const GURL& url);

  // KeyedService:
  void Shutdown() override;

 private:
  void OnAccuracyTipClosed(base::TimeTicks time_opened,
                           AccuracyTipUI::Interaction interaction);

  std::unique_ptr<AccuracyTipUI> ui_;
  scoped_refptr<AccuracyTipSafeBrowsingClient> sb_client_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  GURL sample_url_;

  base::WeakPtrFactory<AccuracyService> weak_factory_{this};
};

}  // namespace accuracy_tips

#endif  // COMPONENTS_ACCURACY_TIPS_ACCURACY_SERVICE_H_
