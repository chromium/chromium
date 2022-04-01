// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_COMPONENTS_RECOVERY_COMPONENT_H_
#define CHROME_CHROME_CLEANER_COMPONENTS_RECOVERY_COMPONENT_H_

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/chrome_cleaner/components/component_api.h"
#include "url/gurl.h"

namespace chrome_cleaner {

class HttpAgentFactory;

// This class starts downloading the recovery component before the scanner
// starts, and it runs it after the cleaner completed, except if a reboot is
// pending.
class RecoveryComponent : public ComponentAPI {
 public:
  // Return true when the recovery component is available for the current build
  // and command line flags combination.
  static bool IsAvailable();

  RecoveryComponent();
  ~RecoveryComponent() override = default;

  // Replace the HttpAgent factory with a new factory. Passing in an empty
  // factory (nullptr) will reset to the default factory. Exposed so tests can
  // create mock HttpAgent objects. This method is not thread-safe.
  static void SetHttpAgentFactoryForTesting(const HttpAgentFactory* factory);

  // ComponentAPI methods.
  void PreScan() override;
  void PostScan(const std::vector<UwSId>& found_pups) override;
  void PreCleanup() override;
  void PostCleanup(ResultCode result_code, RebooterAPI* rebooter) override;
  void PostValidation(ResultCode result_code) override;
  void OnClose(ResultCode result_code) override;

  // Exposed for testing. The size of the buffer used when reading data from the
  // response.
  static const size_t kReadDataFromResponseBufferSize = 8192;

 protected:
  // Try to run the recovery component if it's ready, or wait for it. Protected
  // virtual to test the logic of the public methods.
  virtual void Run();

  // A protected abstraction of the unpacking so that tests can override it.
  virtual void UnpackComponent(const base::FilePath& crx_file);

  // Return when |FetchOnIOThread| is done. Mainly used by tests.
  void WaitForDoneExpandingCrxForTest() { done_expanding_crx_.Wait(); }

 private:
  // The fetch must be done on the IO thread as it's synchronous.
  void FetchOnIOThread();

  // Thread on which the recovery component is fetched, and the CRX is unpacked.
  base::Thread recovery_io_thread_;

  // Where the downloaded CRX was unzipped.
  base::ScopedTempDir component_path_;

  // Signaled when the expansion of the crx is complete.
  base::WaitableEvent done_expanding_crx_;

  // To check whether the component already ran or not.
  bool ran_ = false;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_COMPONENTS_RECOVERY_COMPONENT_H_
