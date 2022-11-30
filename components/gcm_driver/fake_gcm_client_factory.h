// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FAKE_GCM_CLIENT_FACTORY_H_
#define COMPONENTS_GCM_DRIVER_FAKE_GCM_CLIENT_FACTORY_H_

#include "base/compiler_specific.h"
#include "components/gcm_driver/fake_gcm_client.h"
#include "components/gcm_driver/gcm_client_factory.h"

namespace base {
class SequencedTaskRunner;
}

namespace gcm {

class GCMClient;

class FakeGCMClientFactory : public GCMClientFactory {
 public:
  FakeGCMClientFactory(
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const scoped_refptr<base::SequencedTaskRunner>& io_thread);

  FakeGCMClientFactory(const FakeGCMClientFactory&) = delete;
  FakeGCMClientFactory& operator=(const FakeGCMClientFactory&) = delete;

  ~FakeGCMClientFactory() override;

  // GCMClientFactory:
  std::unique_ptr<GCMClient> BuildInstance() override;

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_thread_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FAKE_GCM_CLIENT_FACTORY_H_
