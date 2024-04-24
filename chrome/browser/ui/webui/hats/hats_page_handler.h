// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HATS_HATS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HATS_HATS_PAGE_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/hats/hats.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Interface for a HatsPageDelegate.
// This interface is implemented in classes that want to show and control a HaTS
// WebUI for a survey.
class HatsPageHandlerDelegate {
 public:
  virtual std::string GetTriggerId() = 0;
  virtual bool GetEnableTesting() = 0;
  // Returns a list of preferred languages for the survey (uses BCP47 codes).
  virtual std::vector<std::string> GetLanguageList() = 0;
  // Returns the product specific data associated with this survey.
  virtual base::Value::Dict GetProductSpecificDataJson() = 0;
  virtual void OnSurveyLoaded() = 0;
  virtual void OnSurveyCompleted() = 0;
  virtual void OnSurveyClosed() = 0;
};

class HatsPageHandler : public hats::mojom::PageHandler {
 public:
  HatsPageHandler(mojo::PendingReceiver<hats::mojom::PageHandler> receiver,
                  mojo::PendingRemote<hats::mojom::Page> page,
                  HatsPageHandlerDelegate* delegate);

  HatsPageHandler(const HatsPageHandler&) = delete;
  HatsPageHandler& operator=(const HatsPageHandler&) = delete;

  ~HatsPageHandler() override;

  // hats::mojom::PageHandler:
  void OnSurveyLoaded() override;
  void OnSurveyCompleted() override;
  void OnSurveyClosed() override;

 private:
  mojo::Receiver<hats::mojom::PageHandler> receiver_;
  mojo::Remote<hats::mojom::Page> page_;
  raw_ptr<HatsPageHandlerDelegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_HATS_HATS_PAGE_HANDLER_H_
