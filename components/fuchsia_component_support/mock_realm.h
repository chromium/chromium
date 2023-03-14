// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_MOCK_REALM_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_MOCK_REALM_H_

#include <fuchsia/component/cpp/fidl_test_base.h>

#include "base/fuchsia/scoped_service_binding.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sys {
class OutgoingDirectory;
}

namespace fuchsia_component_support {

// A Mock for tests that wish to validate interactions with
// fuchsia.component/Realm.
class MockRealm : public fuchsia::component::testing::Realm_TestBase {
 public:
  // Publishes the instance in the given outgoing directory.
  explicit MockRealm(sys::OutgoingDirectory* outgoing);
  ~MockRealm() override;

  // fuchsia::component::testing::Realm_TestBase:
  MOCK_METHOD(void,
              OpenExposedDir,
              (fuchsia::component::decl::ChildRef child,
               fidl::InterfaceRequest<fuchsia::io::Directory> exposed_dir,
               fuchsia::component::Realm::OpenExposedDirCallback callback),
              (override));
  MOCK_METHOD(void,
              CreateChild,
              (fuchsia::component::decl::CollectionRef collection,
               fuchsia::component::decl::Child decl,
               fuchsia::component::CreateChildArgs args,
               fuchsia::component::Realm::CreateChildCallback callback),
              (override));
  MOCK_METHOD(void,
              DestroyChild,
              (fuchsia::component::decl::ChildRef child,
               fuchsia::component::Realm::DestroyChildCallback callback),
              (override));
  void NotImplemented_(const std::string& name) override;

 private:
  base::ScopedServiceBinding<fuchsia::component::Realm> binding_;
};

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_MOCK_REALM_H_
