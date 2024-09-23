// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/dynamic_component_host.h"

#include <fuchsia/io/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/remote_dir.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"

namespace fuchsia_component_support {

namespace {

constexpr char kDynamicComponentCapabilitiesPath[] =
    "for_dynamic_component_host";

vfs::PseudoDir* DynamicComponentCapabilitiesDir() {
  return base::ComponentContextForProcess()->outgoing()->GetOrCreateDirectory(
      kDynamicComponentCapabilitiesPath);
}

}  // namespace

DynamicComponentHost::DynamicComponentHost(
    std::string_view collection,
    std::string_view child_id,
    std::string_view component_url,
    base::OnceClosure on_teardown,
    fidl::InterfaceHandle<fuchsia::io::Directory> services)
    : DynamicComponentHost(base::ComponentContextForProcess()
                               ->svc()
                               ->Connect<fuchsia::component::Realm>(),
                           collection,
                           child_id,
                           component_url,
                           std::move(on_teardown),
                           std::move(services)) {}

DynamicComponentHost::DynamicComponentHost(
    fuchsia::component::RealmHandle realm,
    std::string_view collection,
    std::string_view child_id,
    std::string_view component_url,
    base::OnceClosure on_teardown,
    fidl::InterfaceHandle<fuchsia::io::Directory> services)
    : collection_(collection),
      child_id_(child_id),
      on_teardown_(std::move(on_teardown)) {
  DCHECK(realm);

  realm_.Bind(std::move(realm));
  realm_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "Realm disconnected";
    if (on_teardown_) {
      std::move(on_teardown_).Run();
    }
  });

  // If there is a service directory then offer it to the Component as "/svc".
  fuchsia::component::CreateChildArgs create_args;
  if (services) {
    // Link the service-directory to offer to the CFv2 component.
    zx_status_t status = DynamicComponentCapabilitiesDir()->AddEntry(
        child_id_, std::make_unique<vfs::RemoteDir>(std::move(services)));
    ZX_CHECK(status == ZX_OK, status);
    create_args.mutable_dynamic_offers()->push_back(
        fuchsia::component::decl::Offer::WithDirectory(std::move(
            fuchsia::component::decl::OfferDirectory()
                .set_source(fuchsia::component::decl::Ref::WithSelf({}))
                .set_source_name(kDynamicComponentCapabilitiesPath)
                .set_subdir(child_id_)
                .set_target_name("svc")
                .set_rights(fuchsia::io::RW_STAR_DIR)
                .set_dependency_type(
                    fuchsia::component::decl::DependencyType::STRONG))));
  }

  // Create a new Component in the collection. This will not cause it to start.
  realm_->CreateChild(
      {
          .name = collection_,
      },
      std::move(fuchsia::component::decl::Child()
                    .set_name(child_id_)
                    .set_url(std::string(component_url))
                    .set_startup(fuchsia::component::decl::StartupMode::LAZY)),
      std::move(create_args), [this](auto create_result) {
        if (!create_result.is_err()) {
          return;
        }

        LOG(ERROR) << "Error creating Component: "
                   << static_cast<int>(create_result.err());

        // Prevent `DestroyChild()` since there is nothing to destroy.
        realm_ = nullptr;

        if (on_teardown_) {
          std::move(on_teardown_).Run();
        }
      });

  // Attach to the capability directory exposed by the Component.
  fuchsia::io::DirectoryHandle exposed;
  realm_->OpenExposedDir(
      {
          .name = child_id_,
          .collection = collection_,
      },
      exposed.NewRequest(), [this](auto open_result) {
        if (!open_result.is_err()) {
          return;
        }

        LOG(ERROR) << "Error opening Component exposed directory: "
                   << static_cast<int>(open_result.err());
        if (on_teardown_) {
          std::move(on_teardown_).Run();
        }
      });

  exposed_ = std::make_unique<sys::ServiceDirectory>(std::move(exposed));

  // Start the Component, by connecting to the Framework-provided Binder.
  // This is also used to watch for the Component being torn-down.
  binder_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(INFO, status) << "Binder disconnected";
    if (on_teardown_) {
      std::move(on_teardown_).Run();
    }
  });
  exposed_->Connect(binder_.NewRequest());

  DVLOG(1) << "Created DynamicComponentHost " << child_id_;
}

DynamicComponentHost::~DynamicComponentHost() {
  Destroy();

  // If a capabilities directory was created for this component then it must
  // be torn-down to ensure that it cannot continue to be used after the
  // component is supposed to have been destroyed.
  zx_status_t status =
      DynamicComponentCapabilitiesDir()->RemoveEntry(child_id_);
  ZX_CHECK(status == ZX_OK || status == ZX_ERR_NOT_FOUND, status)
      << "RemoveEntry()";

  DVLOG(1) << "Deleted DynamicComponentHost " << child_id_;

  // Don't invoke |on_teardown| here, since we're already being deleted.
}

void DynamicComponentHost::Destroy() {
  DVLOG(2) << "Destroy DynamicComponentHost " << child_id_;

  if (realm_) {
    realm_->DestroyChild(
        {
            .name = child_id_,
            .collection = collection_,
        },
        [](auto result) {});
    realm_ = nullptr;
  }
}

}  // namespace fuchsia_component_support
