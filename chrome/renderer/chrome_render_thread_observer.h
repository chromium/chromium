// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if defined(OS_CHROMEOS)
#include "chrome/renderer/chromeos_delayed_callback_group.h"
#include "mojo/public/cpp/bindings/binding.h"
#endif  // defined(OS_CHROMEOS)

namespace content {
class ResourceDispatcherDelegate;
}

namespace visitedlink {
class VisitedLinkSlave;
}

// This class filters the incoming control messages (i.e. ones not destined for
// a RenderView) for Chrome specific messages that the content layer doesn't
// happen.  If a few messages are related, they should probably have their own
// observer.
class ChromeRenderThreadObserver : public content::RenderThreadObserver,
                                   public chrome::mojom::RendererConfiguration {
 public:
#if defined(OS_CHROMEOS)
  // A helper class to handle Mojo calls that need to be dispatched to the IO
  // thread instead of the main thread as is the norm.
  // This class is thread-safe.
  class ChromeOSListener : public chrome::mojom::ChromeOSListener,
                           public base::RefCountedThreadSafe<ChromeOSListener> {
   public:
    static scoped_refptr<ChromeOSListener> Create(
        mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
            chromeos_listener_receiver);

    // Is the merge session still running?
    bool IsMergeSessionRunning() const;

    // Run |callback| on the calling sequence when the merge session has
    // finished (or timed out).
    void RunWhenMergeSessionFinished(DelayedCallbackGroup::Callback callback);

   protected:
    // chrome::mojom::ChromeOSListener:
    void MergeSessionComplete() override;

   private:
    friend class base::RefCountedThreadSafe<ChromeOSListener>;

    ChromeOSListener();
    ~ChromeOSListener() override;

    void BindOnIOThread(mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
                            chromeos_listener_receiver);

    scoped_refptr<DelayedCallbackGroup> session_merged_callbacks_;
    bool merge_session_running_ GUARDED_BY(lock_);
    mutable base::Lock lock_;
    mojo::Receiver<chrome::mojom::ChromeOSListener> receiver_{this};

    DISALLOW_COPY_AND_ASSIGN(ChromeOSListener);
  };
#endif  // defined(OS_CHROMEOS)

  ChromeRenderThreadObserver();
  ~ChromeRenderThreadObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }

  // Return the dynamic parameters - those that may change while the
  // render process is running.
  static const chrome::mojom::DynamicParams& GetDynamicParams();

  // Returns a pointer to the content setting rules owned by
  // |ChromeRenderThreadObserver|.
  const RendererContentSettingRules* content_setting_rules() const;

  visitedlink::VisitedLinkSlave* visited_link_slave() {
    return visited_link_slave_.get();
  }

#if defined(OS_CHROMEOS)
  scoped_refptr<ChromeOSListener> chromeos_listener() const {
    return chromeos_listener_;
  }
#endif  // defined(OS_CHROMEOS)

 private:
  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // chrome::mojom::RendererConfiguration:
  void SetInitialConfiguration(
      bool is_incognito_process,
      mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
          chromeos_listener_receiver) override;
  void SetConfiguration(chrome::mojom::DynamicParamsPtr params) override;
  void SetContentSettingRules(
      const RendererContentSettingRules& rules) override;
  void SetFieldTrialGroup(const std::string& trial_name,
                          const std::string& group_name) override;

  void OnRendererConfigurationAssociatedRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
          receiver);

  static bool is_incognito_process_;
  std::unique_ptr<content::ResourceDispatcherDelegate> resource_delegate_;
  RendererContentSettingRules content_setting_rules_;

  std::unique_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;

  mojo::AssociatedReceiverSet<chrome::mojom::RendererConfiguration>
      renderer_configuration_receivers_;

#if defined(OS_CHROMEOS)
  // Only set if the Chrome OS merge session was running when the renderer
  // was started.
  scoped_refptr<ChromeOSListener> chromeos_listener_;
#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderThreadObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
