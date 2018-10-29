// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/pdf/common/pdf.mojom.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
class WebContents;
}

namespace pdf {

class PDFWebContentsHelperClient;

// Per-WebContents class to handle PDF messages.
class PDFWebContentsHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PDFWebContentsHelper>,
      public mojom::PdfService,
      public ui::TouchSelectionControllerClient,
      public ui::TouchSelectionMenuClient,
      public content::TouchSelectionControllerClientManager::Observer {
 public:
  ~PDFWebContentsHelper() override;

  static void CreateForWebContentsWithClient(
      content::WebContents* contents,
      std::unique_ptr<PDFWebContentsHelperClient> client);

  // ui::TouchSelectionControllerClient :
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override {}
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;

  // ui::TouchSelectionMenuRunner:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  base::string16 GetSelectedText() override;

  // ui::TouchSelectionControllerClientManager::Observer:
  void OnManagerWillDestroy(
      content::TouchSelectionControllerClientManager* manager) override;

 private:
  PDFWebContentsHelper(content::WebContents* web_contents,
                       std::unique_ptr<PDFWebContentsHelperClient> client);

  void InitTouchSelectionClientManager();
  gfx::PointF ConvertFromRoot(const gfx::PointF& point_f) const;
  gfx::PointF ConvertToRoot(const gfx::PointF& point_f) const;
  gfx::PointF ConvertHelper(const gfx::PointF& point_f, float scale) const;

  // mojom::PdfService:
  void SetListener(mojom::PdfListenerPtr listener) override;
  void HasUnsupportedFeature() override;
  void SaveUrlAs(const GURL& url, const content::Referrer& referrer) override;
  void UpdateContentRestrictions(int32_t content_restrictions) override;
  void SelectionChanged(const gfx::PointF& left,
                        int32_t left_height,
                        const gfx::PointF& right,
                        int32_t right_height) override;

  content::WebContentsFrameBindingSet<mojom::PdfService> pdf_service_bindings_;
  std::unique_ptr<PDFWebContentsHelperClient> client_;
  content::TouchSelectionControllerClientManager*
      touch_selection_controller_client_manager_;

  // Latest selection bounds received from PDFium.
  gfx::PointF selection_left_;
  int32_t selection_left_height_;
  gfx::PointF selection_right_;
  int32_t selection_right_height_;
  bool has_selection_;

  mojom::PdfListenerPtr remote_pdf_client_;
  // Not owned.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(PDFWebContentsHelper);
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_
