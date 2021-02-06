// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_UIA_ACCESSIBILITY_EVENT_WAITER_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_UIA_ACCESSIBILITY_EVENT_WAITER_H_

#include <ole2.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>

#include <map>
#include <string>
#include <vector>

#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/atl.h"
#include "ui/views/accessibility/view_accessibility.h"

struct UiaAccessibilityWaiterInfo {
  HWND hwnd;
  base::string16 role;
  base::string16 name;
  ax::mojom::Event event;
};

class UiaAccessibilityEventWaiter {
 public:
  explicit UiaAccessibilityEventWaiter(UiaAccessibilityWaiterInfo info);
  ~UiaAccessibilityEventWaiter();

  void Wait();
  void WaitWithTimeout(base::TimeDelta timeout);

 private:
  // All UIA calls need to be made on a secondary MTA thread to avoid sporadic
  // test hangs / timeouts.
  class Thread : public base::PlatformThread::Delegate {
   public:
    Thread();
    ~Thread() override;

    void Init(UiaAccessibilityEventWaiter* owner,
              const UiaAccessibilityWaiterInfo& info,
              base::OnceClosure initialization_loop,
              base::OnceClosure shutdown_loop);

    void SendShutdownSignal();

    void ThreadMain() override;

   protected:
    UiaAccessibilityWaiterInfo info_;

   private:
    UiaAccessibilityEventWaiter* owner_ = nullptr;

    Microsoft::WRL::ComPtr<IUIAutomation> uia_;
    Microsoft::WRL::ComPtr<IUIAutomationElement> root_;
    Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request_;

    // Thread synchronization members.
    base::OnceClosure initialization_complete_;
    base::OnceClosure shutdown_complete_;
    base::WaitableEvent shutdown_signal_;

    // An implementation of various UIA interfaces that forward event
    // notifications to the waiter.
    class EventHandler : public CComObjectRootEx<CComMultiThreadModel>,
                         public IUIAutomationFocusChangedEventHandler,
                         public IUIAutomationPropertyChangedEventHandler,
                         public IUIAutomationStructureChangedEventHandler,
                         public IUIAutomationEventHandler {
     public:
      EventHandler();
      virtual ~EventHandler();

      void Init(UiaAccessibilityEventWaiter::Thread* owner,
                Microsoft::WRL::ComPtr<IUIAutomationElement> root);
      void CleanUp();

      BEGIN_COM_MAP(EventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationFocusChangedEventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationPropertyChangedEventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationStructureChangedEventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationEventHandler)
      END_COM_MAP()

      // IUIAutomationFocusChangedEventHandler interface.
      IFACEMETHODIMP HandleFocusChangedEvent(
          IUIAutomationElement* sender) override;

      // IUIAutomationPropertyChangedEventHandler interface.
      IFACEMETHODIMP HandlePropertyChangedEvent(IUIAutomationElement* sender,
                                                PROPERTYID property_id,
                                                VARIANT new_value) override;

      // IUIAutomationStructureChangedEventHandler interface.
      IFACEMETHODIMP HandleStructureChangedEvent(
          IUIAutomationElement* sender,
          StructureChangeType change_type,
          SAFEARRAY* runtime_id) override;

      // IUIAutomationEventHandler interface.
      IFACEMETHODIMP HandleAutomationEvent(IUIAutomationElement* sender,
                                           EVENTID event_id) override;

      // Points to the waiter to receive notifications.
      UiaAccessibilityEventWaiter::Thread* owner_ = nullptr;

     private:
      bool MatchesNameRole(IUIAutomationElement* sender);

      Microsoft::WRL::ComPtr<IUIAutomationElement> root_;

      DISALLOW_COPY_AND_ASSIGN(EventHandler);
    };
    Microsoft::WRL::ComPtr<CComObject<EventHandler>> uia_event_handler_;
  };

  Thread thread_;
  base::RunLoop shutdown_loop_;
  base::PlatformThreadHandle thread_handle_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_UIA_ACCESSIBILITY_EVENT_WAITER_H_
