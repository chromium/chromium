// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_

#import <map>
#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/foundations/autofill_client.h"
#import "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#import "components/autofill/core/browser/foundations/autofill_driver_router.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace autofill {

class AutofillClientIOS;
class AutofillDriverIOS;

// Creates one AutofillDriverIOS per web::WebState and manages its lifecycle
// corresponding to the web::WebState's lifecycle.
//
// Owned by AutofillClientIOS, therefore there is one AutofillDriverIOSFactory
// per web::WebState.
class AutofillDriverIOSFactory final
    : public AutofillDriverFactory,
      public web::WebStateObserver,
      public web::WebFramesManager::Observer {
 public:
  // A variant of AutofillDriverFactory::Observer with AutofillDriver[Factory]
  // narrowed to AutofillDriverIOS[Factory].
  // See AutofillDriverFactory::Observer for further documentation.
  class Observer : public AutofillDriverFactory::Observer {
   public:
    virtual void OnAutofillDriverIOSFactoryDestroyed(
        AutofillDriverIOSFactory& factory) {}
    virtual void OnAutofillDriverIOSCreated(AutofillDriverIOSFactory& factory,
                                            AutofillDriverIOS& driver) {}
    virtual void OnAutofillDriverIOSStateChanged(
        AutofillDriverIOSFactory& factory,
        AutofillDriverIOS& driver,
        AutofillDriver::LifecycleState old_state,
        AutofillDriver::LifecycleState new_state) {}

    // AutofillDriverFactory::Observer:
    void OnAutofillDriverFactoryDestroyed(AutofillDriverFactory& factory) final;
    void OnAutofillDriverCreated(AutofillDriverFactory& factory,
                                 AutofillDriver& driver) final;
    void OnAutofillDriverStateChanged(AutofillDriverFactory& factory,
                                      AutofillDriver& driver,
                                      LifecycleState old_state,
                                      LifecycleState new_state) final;
  };

  AutofillDriverIOSFactory(AutofillClientIOS* client,
                           id<AutofillDriverIOSBridge> bridge);

  ~AutofillDriverIOSFactory() override;

  // Returns the AutofillDriverIOS for `web_frame`. Creates the driver if
  // necessary.
  AutofillDriverIOS* DriverForFrame(web::WebFrame* web_frame);

  AutofillDriverRouter& router() { return router_; }

  std::vector<AutofillDriver*> GetExistingDrivers() override;

 private:
  friend class AutofillDriverIOSFactoryTestApi;

  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::WebFramesManager::Observer:
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;
  void WebFrameBecameUnavailable(web::WebFramesManager* web_frames_manager,
                                 const std::string& frame_id) override;

  web::WebFramesManager& GetWebFramesManager();
  web::WebState* web_state();

  raw_ref<AutofillClientIOS> client_;
  AutofillDriverRouter router_;
  id<AutofillDriverIOSBridge> bridge_ = nil;

  // Indicates whether WebStateDestroyed() has been fired already to prevent
  // that subsequent DriverForFrame() calls create new drivers.
  bool web_state_destroyed_ = false;

  // Owns the drivers. Drivers are created lazily in DriverForFrame() and
  // destroyed in WebFrameBecameUnavailable(). An entry with a null driver
  // indicates that the frame is currently unavailable.
  //
  // The purpose of these null entries is to prevent DriverForFrame() from
  // creating drivers for unavailable frames. (This can happen if another
  // WebFrameBecameUnavailable() handler calls DriverForFrame() after the
  // factory's WebFrameBecameUnavailable() deleted the driver.) The null entry
  // is cleaned up in WebFrameBecameAvailable() once the frame has been fully
  // removed.
  //
  // This differs from ContentAutofillDriverFactory, which instead inspects the
  // lifecycle state of the RenderFrameHost to decide if an AutofillDriver can
  // be created for the frame. Unlike RenderFrameHost, WebFrames do not expose a
  // lifecycle state.
  //
  // The map type must be so that `driver_map_.emplace()` does *not* invalidate
  // references. Otherwise, recursive DriverForFrame() calls are unsafe.
  std::map<std::string, std::unique_ptr<AutofillDriverIOS>> driver_map_;

  // The maximum number of coexisting drivers over the lifetime of this factory.
  // TODO: crbug.com/365097975 - Remove the counter and the metric.
  size_t max_drivers_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_DRIVER_IOS_FACTORY_H_
