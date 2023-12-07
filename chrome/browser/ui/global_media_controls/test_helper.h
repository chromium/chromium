// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_

#include "base/observer_list.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "content/public/browser/presentation_request.h"
#include "testing/gmock/include/gmock/gmock.h"

using media_router::WebContentsPresentationManager;

class MockWebContentsPresentationManager
    : public WebContentsPresentationManager {
 public:
  MockWebContentsPresentationManager();
  ~MockWebContentsPresentationManager() override;

  void NotifyMediaRoutesChanged(
      const std::vector<media_router::MediaRoute>& routes);
  void SetDefaultPresentationRequest(
      const content::PresentationRequest& request);

  // WebContentsPresentationManager implementation.
  bool HasDefaultPresentationRequest() const override;
  const content::PresentationRequest& GetDefaultPresentationRequest()
      const override;
  void AddObserver(content::PresentationObserver* observer) override;
  void RemoveObserver(content::PresentationObserver* observer) override;
  base::WeakPtr<WebContentsPresentationManager> GetWeakPtr() override;

  MOCK_METHOD(void,
              OnPresentationResponse,
              (const content::PresentationRequest&,
               media_router::mojom::RoutePresentationConnectionPtr,
               const media_router::RouteRequestResult&));
  MOCK_METHOD(std::vector<media_router::MediaRoute>, GetMediaRoutes, ());

 private:
  std::optional<content::PresentationRequest> default_presentation_request_;
  base::ObserverList<content::PresentationObserver> observers_;
  base::WeakPtrFactory<MockWebContentsPresentationManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
