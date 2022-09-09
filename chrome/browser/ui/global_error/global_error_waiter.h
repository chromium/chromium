// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_WAITER_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/global_error/global_error_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"

class Profile;

namespace test {

// A helper class to wait for a GlobalError change notification from
// GlobalErrorService.
class GlobalErrorWaiter : public GlobalErrorObserver {
 public:
  explicit GlobalErrorWaiter(Profile* profile);

  GlobalErrorWaiter(const GlobalErrorWaiter&) = delete;
  GlobalErrorWaiter& operator=(const GlobalErrorWaiter&) = delete;

  ~GlobalErrorWaiter() override;

  // GlobalErrorObserver:
  void OnGlobalErrorsChanged() override;

  // Wait() will return once a notification has been observed. It will return
  // immediately if one has already been seen.
  void Wait();

 private:
  bool errors_changed_ = false;
  base::RunLoop run_loop_;
  base::ScopedObservation<GlobalErrorService, GlobalErrorObserver>
      scoped_observation_{this};
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_WAITER_H_
