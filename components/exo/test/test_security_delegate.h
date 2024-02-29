// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_
#define COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_

#include "components/exo/security_delegate.h"

#include "url/gurl.h"

namespace aura {
class Window;
}

namespace exo::test {

class TestSecurityDelegate : public SecurityDelegate {
 public:
  TestSecurityDelegate();
  TestSecurityDelegate(const TestSecurityDelegate&) = delete;
  TestSecurityDelegate& operator=(const TestSecurityDelegate&) = delete;
  ~TestSecurityDelegate() override;

  // SecurityDelegate:
  bool CanSelfActivate(aura::Window* window) const override;
  bool CanLockPointer(aura::Window* toplevel) const override;
  SetBoundsPolicy CanSetBounds(aura::Window* window) const override;
  std::vector<ui::FileInfo> GetFilenames(
      ui::EndpointType source,
      const std::vector<uint8_t>& data) const override;
  void SendFileInfo(ui::EndpointType target,
                    const std::vector<ui::FileInfo>& files,
                    SendDataCallback callback) const override;
  void SendPickle(ui::EndpointType target,
                  const base::Pickle& pickle,
                  SendDataCallback callback) override;

  // Choose the return value of |CanSetBounds()|.
  void SetCanSetBounds(SetBoundsPolicy policy);

  // Run the callback received in SendPickle() with the specified values..
  void RunSendPickleCallback(std::vector<GURL> urls);

 protected:
  SetBoundsPolicy policy_ = SetBoundsPolicy::IGNORE;
  SendDataCallback send_pickle_callback_;
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_
