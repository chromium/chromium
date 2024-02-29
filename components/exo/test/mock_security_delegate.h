// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_MOCK_SECURITY_DELEGATE_H_
#define COMPONENTS_EXO_TEST_MOCK_SECURITY_DELEGATE_H_

#include "components/exo/security_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/clipboard/file_info.h"

namespace exo::test {

class MockSecurityDelegate : public SecurityDelegate {
 public:
  MockSecurityDelegate();
  ~MockSecurityDelegate() override;

  MOCK_METHOD(bool, CanSelfActivate, (aura::Window*), (const, override));
  MOCK_METHOD(bool, CanLockPointer, (aura::Window*), (const, override));
  MOCK_METHOD(SetBoundsPolicy,
              CanSetBounds,
              (aura::Window * window),
              (const, override));
  MOCK_METHOD(std::vector<ui::FileInfo>,
              GetFilenames,
              (ui::EndpointType source, const std::vector<uint8_t>& data),
              (const, override));
  MOCK_METHOD(void,
              SendFileInfo,
              (ui::EndpointType target,
               const std::vector<ui::FileInfo>& files,
               SendDataCallback callback),
              (const, override));
  MOCK_METHOD(void,
              SendPickle,
              (ui::EndpointType target,
               const base::Pickle& pickle,
               SendDataCallback callback),
              (override));
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_MOCK_SECURITY_DELEGATE_H_
