// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SANDBOX_STATUS_EXTENSION_ANDROID_H_
#define CHROME_RENDERER_SANDBOX_STATUS_EXTENSION_ANDROID_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/common/sandbox_status_extension_android.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

// On Android, this class adds a function chrome.getAndroidSandboxStatus()
// to the chrome://sandbox/ WebUI page. This is done only after the browser
// SandboxInternalsUI sends an IPC mesage blessing this RenderFrame.
class SandboxStatusExtension
    : public base::RefCountedThreadSafe<SandboxStatusExtension>,
      public content::RenderFrameObserver,
      public chrome::mojom::SandboxStatusExtension {
 public:
  // Creates a new SandboxStatusExtension for the |frame|.
  static void Create(content::RenderFrame* frame);

  SandboxStatusExtension(const SandboxStatusExtension&) = delete;
  SandboxStatusExtension& operator=(const SandboxStatusExtension&) = delete;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidClearWindowObject() override;

 protected:
  friend class base::RefCountedThreadSafe<SandboxStatusExtension>;
  ~SandboxStatusExtension() override;

 private:
  explicit SandboxStatusExtension(content::RenderFrame* frame);

  // chrome::mojom::SandboxStatusExtension
  void AddSandboxStatusExtension() override;

  void OnSandboxStatusExtensionRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::SandboxStatusExtension>
          receiver);

  // Installs the JavaScript function into the scripting context, if
  // should_install_ is true.
  void Install();

  // Native implementation of chrome.getAndroidSandboxStatus.
  void GetSandboxStatus(gin::Arguments* args);

  // Called on the blocking pool, this gets the sandbox status of the current
  // renderer process and returns a status object as a base::Value::Dict.
  base::Value::Dict ReadSandboxStatus();

  // Runs the callback argument provided to GetSandboxStatus() with the status
  // object computed by ReadSandboxStatus(). This is called back on the thread
  // on which GetSandboxStatus() was called originally.
  void RunCallback(std::unique_ptr<v8::Global<v8::Function>> callback,
                   base::Value::Dict status);

  // Set to true by AddSandboxStatusExtension().
  bool should_install_ = false;

  mojo::AssociatedReceiver<chrome::mojom::SandboxStatusExtension> receiver_{
      this};
};

#endif  // CHROME_RENDERER_SANDBOX_STATUS_EXTENSION_ANDROID_H_
