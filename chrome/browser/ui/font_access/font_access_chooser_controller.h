// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROME_BROWSER_UI_FONT_ACCESS_FONT_ACCESS_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_FONT_ACCESS_FONT_ACCESS_CHOOSER_CONTROLLER_H_

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "content/public/browser/font_access_chooser.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom-shared.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

namespace content {

class RenderFrameHost;

}  // namespace content

class FontAccessChooserController : public ChooserController {
 public:
  FontAccessChooserController(content::RenderFrameHost* render_frame_host,
                              const std::vector<std::string>& selection,
                              content::FontAccessChooser::Callback callback);
  ~FontAccessChooserController() override;

  // Disallow copy and assign.
  FontAccessChooserController(FontAccessChooserController&) = delete;
  FontAccessChooserController& operator=(FontAccessChooserController&) = delete;

  // ChooserController:
  std::u16string GetNoOptionsText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  size_t NumOptions() const override;
  std::u16string GetOption(size_t index) const override;
  std::u16string GetSelectAllCheckboxLabel() const override;

  bool ShouldShowHelpButton() const override;
  bool ShouldShowReScanButton() const override;
  bool BothButtonsAlwaysEnabled() const override;
  bool TableViewAlwaysDisabled() const override;
  bool AllowMultipleSelection() const override;
  bool ShouldShowSelectAllCheckbox() const override;

  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  void SetReadyCallbackForTesting(base::OnceClosure callback) {
    ready_callback_for_testing_ = std::move(callback);
  }

 private:
  void DidFindAllFonts(blink::mojom::FontEnumerationStatus status,
                       std::vector<blink::mojom::FontMetadata> fonts);
  content::FontAccessChooser::Callback callback_;
  base::OnceClosure ready_callback_for_testing_;

  std::map<std::string, blink::mojom::FontMetadata> font_metadata_map_;
  // An ordered list of font names that determines the order of items in the
  // chooser.
  std::vector<std::string> items_;

  // If supplied, this will limit the choices the user gets to see to
  // those in this list.
  base::flat_set<std::string> selection_;

  base::WeakPtrFactory<FontAccessChooserController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_FONT_ACCESS_FONT_ACCESS_CHOOSER_CONTROLLER_H_
