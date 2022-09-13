// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_NACL_HELPER_H_
#define COMPONENTS_NACL_RENDERER_NACL_HELPER_H_

#include "base/compiler_specific.h"
#include "content/public/renderer/render_frame_observer.h"

namespace nacl {

// This class listens for Pepper creation events from the RenderFrame. For the
// NaCl trusted plugin, it configures it as an external plugin host.
// TODO(dmichael): When the trusted plugin goes away, we need to figure out the
//                 right event to watch for.
class NaClHelper : public content::RenderFrameObserver {
 public:
  explicit NaClHelper(content::RenderFrame* render_frame);

  NaClHelper(const NaClHelper&) = delete;
  NaClHelper& operator=(const NaClHelper&) = delete;

  ~NaClHelper() override;

  // RenderFrameObserver.
  void DidCreatePepperPlugin(content::RendererPpapiHost* host) override;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_NACL_HELPER_H_
