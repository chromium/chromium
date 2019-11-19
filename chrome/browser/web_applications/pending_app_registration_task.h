// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_REGISTRATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_REGISTRATION_TASK_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "content/public/browser/service_worker_context_observer.h"

class GURL;

namespace content {
enum class ServiceWorkerCapability;
class ServiceWorkerContext;
class WebContents;
}  // namespace content

namespace web_app {

enum class RegistrationResultCode;
class WebAppUrlLoader;

class PendingAppRegistrationTaskBase
    : public content::ServiceWorkerContextObserver {
 public:
  ~PendingAppRegistrationTaskBase() override;

  const GURL& launch_url() const { return launch_url_; }

 protected:
  explicit PendingAppRegistrationTaskBase(const GURL& launch_url);

 private:
  const GURL launch_url_;
};

class PendingAppRegistrationTask : public PendingAppRegistrationTaskBase {
 public:
  using RegistrationCallback = base::OnceCallback<void(RegistrationResultCode)>;

  PendingAppRegistrationTask(const GURL& launch_url,
                             WebAppUrlLoader* url_loader,
                             content::WebContents* web_contents,
                             RegistrationCallback callback);
  ~PendingAppRegistrationTask() override;

  // ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& scope) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  static void SetTimeoutForTesting(int registration_timeout_in_seconds);

 private:
  void OnDidCheckHasServiceWorker(content::ServiceWorkerCapability capability);

  void OnRegistrationTimeout();

  WebAppUrlLoader* const url_loader_;
  content::WebContents* const web_contents_;
  RegistrationCallback callback_;
  content::ServiceWorkerContext* service_worker_context_;

  base::OneShotTimer registration_timer_;

  base::WeakPtrFactory<PendingAppRegistrationTask> weak_ptr_factory_{this};

  static int registration_timeout_in_seconds_;

  DISALLOW_COPY_AND_ASSIGN(PendingAppRegistrationTask);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PENDING_APP_REGISTRATION_TASK_H_
