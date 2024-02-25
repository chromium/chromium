// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_FONT_PREWARMER_H_
#define CHROME_RENDERER_FONT_PREWARMER_H_

#include "chrome/common/font_prewarmer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

// Trivial forwards request from chrome::mojom::FontPrewarmer to blink.
class FontPrewarmer : public chrome::mojom::FontPrewarmer {
 public:
  FontPrewarmer(const FontPrewarmer&) = delete;
  FontPrewarmer& operator=(const FontPrewarmer&) = delete;

  static void Bind(
      mojo::PendingReceiver<chrome::mojom::FontPrewarmer> pending_receiver);

  // chrome::mojom::FontPrewarmer:
  void PrewarmFonts(const std::vector<std::string>& font_names) override;

 private:
  FontPrewarmer(
      mojo::PendingReceiver<chrome::mojom::FontPrewarmer> pending_receiver);
  ~FontPrewarmer() override;

  void OnDisconnect();

  mojo::Receiver<chrome::mojom::FontPrewarmer> receiver_;
};

#endif  // CHROME_RENDERER_FONT_PREWARMER_H_
