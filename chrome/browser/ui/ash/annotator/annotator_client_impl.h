// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ANNOTATOR_ANNOTATOR_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ANNOTATOR_ANNOTATOR_CLIENT_IMPL_H_

#include "ash/public/cpp/annotator/annotator_controller_base.h"
#include "ash/public/cpp/annotator/annotator_tool.h"
#include "ash/webui/annotator/public/cpp/annotator_client.h"
#include "ash/webui/annotator/untrusted_annotator_page_handler_impl.h"
#include "base/memory/raw_ptr.h"

namespace views {
class WebView;
}  // namespace views

// Implements the interface for the Annotator tool.
class AnnotatorClientImpl : public ash::AnnotatorClient {
 public:
  explicit AnnotatorClientImpl(
      ash::AnnotatorControllerBase* annotator_controller);
  AnnotatorClientImpl();
  AnnotatorClientImpl(const AnnotatorClientImpl&) = delete;
  AnnotatorClientImpl& operator=(const AnnotatorClientImpl&) = delete;
  ~AnnotatorClientImpl() override;

  // RecordingOverlayViewImpl calls this function to initialize the annotator
  // tool.
  static void InitForProjectorAnnotator(views::WebView* web_view);

  // ash::AnnotatorClient:
  void SetAnnotatorPageHandler(
      ash::UntrustedAnnotatorPageHandlerImpl* handler) override;
  void ResetAnnotatorPageHandler(
      ash::UntrustedAnnotatorPageHandlerImpl* handler) override;
  void SetTool(const ash::AnnotatorTool& tool) override;
  void Clear() override;
  std::unique_ptr<ash::AnnotationsOverlayView> CreateAnnotationsOverlayView()
      const override;

  ash::UntrustedAnnotatorPageHandlerImpl* get_annotator_handler_for_test() {
    return annotator_handler_;
  }

 private:
  raw_ptr<ash::AnnotatorControllerBase> annotator_controller_ = nullptr;
  raw_ptr<ash::UntrustedAnnotatorPageHandlerImpl> annotator_handler_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_ANNOTATOR_ANNOTATOR_CLIENT_IMPL_H_
