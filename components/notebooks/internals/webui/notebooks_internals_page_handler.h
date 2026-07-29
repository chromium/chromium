// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_PAGE_HANDLER_H_
#define COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/notebooks/internals/webui/notebooks_internals.mojom.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}

namespace notebooks {

class NotebooksInternalsPageHandler
    : public notebooks_internals::mojom::PageHandler,
      public notebooks::NotebooksEligibilityService::Observer {
 public:
  NotebooksInternalsPageHandler(
      mojo::PendingReceiver<notebooks_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<notebooks_internals::mojom::Page> page,
      content::BrowserContext* browser_context,
      NotebooksEligibilityService* notebooks_eligibility_service);
  ~NotebooksInternalsPageHandler() override;

  NotebooksInternalsPageHandler(const NotebooksInternalsPageHandler&) = delete;
  NotebooksInternalsPageHandler& operator=(
      const NotebooksInternalsPageHandler&) = delete;

  // notebooks_internals::mojom::PageHandler:
  void GetFeatureFlagState(GetFeatureFlagStateCallback callback) override;
  void GetProfileEligibility(GetProfileEligibilityCallback callback) override;

  // notebooks::NotebooksEligibilityService::Observer:
  void OnNotebooksEligibilityChanged(bool eligible) override;

 private:
  mojo::Receiver<notebooks_internals::mojom::PageHandler> receiver_;
  mojo::Remote<notebooks_internals::mojom::Page> page_;
  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<NotebooksEligibilityService> notebooks_eligibility_service_;
  base::ScopedObservation<NotebooksEligibilityService,
                          NotebooksEligibilityService::Observer>
      notebooks_eligibility_service_observation_{this};
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_PAGE_HANDLER_H_
