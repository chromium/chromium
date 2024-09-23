// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OOBE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OOBE_UI_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/core_oobe.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_screens_handler_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-forward.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ui {
class ColorChangeHandler;
}

namespace color_change_listener::mojom {
class PageHandler;
}  // namespace color_change_listener::mojom

namespace content {
class WebUIDataSource;
}

namespace ash {

class ErrorScreen;
class NetworkStateInformer;
class OobeDisplayChooser;
class OobeUI;

// The WebUIConfig for chrome://oobe urls
class OobeUIConfig : public ChromeOSWebUIConfig<OobeUI> {
 public:
  OobeUIConfig()
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            chrome::kChromeUIOobeHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// A custom WebUI that defines datasource for out-of-box-experience (OOBE) UI:
// - welcome screen (setup language/keyboard/network).
// - eula screen (CrOS (+ OEM) EULA content/TPM password/crash reporting).
// - update screen.
class OobeUI : public ui::MojoWebUIController {
 public:
  // List of known types of OobeUI. Type added as path in chrome://oobe url, for
  // example chrome://oobe/gaia-signin.
  static inline constexpr char kAppLaunchSplashDisplay[] = "app-launch-splash";
  static inline constexpr char kGaiaSigninDisplay[] = "gaia-signin";
  static inline constexpr char kOobeDisplay[] = "oobe";
  static inline constexpr char kOobeTestLoader[] = "test_loader.html";

  class Observer {
   public:
    Observer() = default;

    Observer(const Observer&) = delete;

    virtual void OnCurrentScreenChanged(OobeScreenId current_screen,
                                        OobeScreenId new_screen) = 0;

    virtual void OnBackdropLoaded() {}
    virtual void OnDestroyingOobeUI() = 0;

   protected:
    virtual ~Observer() = default;
  };

  OobeUI(content::WebUI* web_ui, const GURL& url);

  OobeUI(const OobeUI&) = delete;
  OobeUI& operator=(const OobeUI&) = delete;

  ~OobeUI() override;

  CoreOobe* GetCoreOobe();
  ErrorScreen* GetErrorScreen();
  OobeScreensHandlerFactory* GetOobeScreensHandlerFactory();

  // Collects localized strings from the owned handlers.
  base::Value::Dict GetLocalizedStrings();

  // Initializes the handlers.
  void InitializeHandlers();

  // Called when the screen has changed.
  void CurrentScreenChanged(OobeScreenId screen);

  // Called when the backdrop image of the OOBE is loaded.
  void OnBackdropLoaded();

  bool IsJSReady(base::OnceClosure display_is_ready_callback);

  gfx::NativeView GetNativeView();

  gfx::NativeWindow GetTopLevelNativeWindow();

  gfx::Size GetViewSize();

  // Add and remove observers for screen change events.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  OobeScreenId current_screen() const { return current_screen_; }

  OobeScreenId previous_screen() const { return previous_screen_; }

  const std::string& display_type() const { return display_type_; }

  NetworkStateInformer* network_state_informer_for_test() const {
    return network_state_informer_.get();
  }

  // Re-evaluate OOBE display placement.
  void OnDisplayConfigurationChanged();

  // Find a *View instance provided by a given *Handler type.
  //
  // This is the same as GetHandler() except the return type is limited to the
  // view.
  template <typename THandler>
  typename THandler::TView* GetView() {
    return GetHandler<THandler>();
  }

  // Find a handler instance.
  template <typename THandler>
  THandler* GetHandler() {
    OobeScreenId expected_screen = THandler::kScreenId;
    for (BaseScreenHandler* handler : screen_handlers_) {
      if (expected_screen == handler->oobe_screen())
        return static_cast<THandler*>(handler);
    }

    NOTREACHED_IN_MIGRATION()
        << "Unable to find handler for screen " << expected_screen;
    return nullptr;
  }

  // Instantiates implementor of the mojom::MultiDeviceSetup mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<ash::multidevice_setup::mojom::MultiDeviceSetup>
          receiver);
  // Instantiates implementor of the mojom::PrivilegedHostDeviceSetter mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          ash::multidevice_setup::mojom::PrivilegedHostDeviceSetter> receiver);
  // Instantiates implementor of the mojom::CrosNetworkConfig mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver);

  // Instantiates implementor of the mojom::ESimManager mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<ash::cellular_setup::mojom::ESimManager> receiver);

  // Binds to the Jelly dynamic color Mojo
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // Binds to the cros authentication factor editing services.
  void BindInterface(
      mojo::PendingReceiver<auth::mojom::AuthFactorConfig> receiver);
  void BindInterface(
      mojo::PendingReceiver<auth::mojom::PinFactorEditor> receiver);
  void BindInterface(
      mojo::PendingReceiver<auth::mojom::PasswordFactorEditor> receiver);

  void BindInterface(
      mojo::PendingReceiver<screens_factory::mojom::ScreensFactory> receiver);

  static void AddOobeComponents(content::WebUIDataSource* source);

  bool ready() const { return ready_; }

 private:
  void AddWebUIHandler(std::unique_ptr<BaseWebUIHandler> handler);
  void AddScreenHandler(std::unique_ptr<BaseScreenHandler> handler);

  // Configures all the relevant screen shandlers and resources for OOBE/Login
  // display type.
  void ConfigureOobeDisplay();

  // Updates default scaling for CfM devices.
  void UpScaleOobe();
  bool ShouldUpScaleOobe();

  // Type of UI.
  std::string display_type_;

  // Reference to NetworkStateInformer that handles changes in network
  // state.
  scoped_refptr<NetworkStateInformer> network_state_informer_;

  // Reference to CoreOobeHandler that handles common requests of Oobe page.
  raw_ptr<CoreOobeHandler> core_handler_ = nullptr;
  std::unique_ptr<CoreOobe> core_oobe_;

  std::vector<raw_ptr<BaseWebUIHandler, VectorExperimental>>
      webui_handlers_;  // Non-owning pointers.
  std::vector<raw_ptr<BaseWebUIHandler, VectorExperimental>>
      webui_only_handlers_;  // Non-owning pointers.
  std::vector<raw_ptr<BaseScreenHandler, VectorExperimental>>
      screen_handlers_;  // Non-owning pointers.

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  std::unique_ptr<OobeScreensHandlerFactory> oobe_screens_handler_factory_;

  std::unique_ptr<ErrorScreen> error_screen_;

  // Id of the current oobe/login screen.
  OobeScreenId current_screen_ = ash::OOBE_SCREEN_UNKNOWN;

  // Id of the previous oobe/login screen.
  OobeScreenId previous_screen_ = ash::OOBE_SCREEN_UNKNOWN;

  // Id of display that was already scaled for CfM devices.
  int64_t upscaled_display_id_ = -1;

  // Flag that indicates whether JS part is fully loaded and ready to accept
  // calls.
  bool ready_ = false;

  // Callbacks to notify when JS part is fully loaded and ready to accept calls.
  base::OnceClosureList ready_callbacks_;

  // List of registered observers.
  base::ObserverList<Observer>::Unchecked observer_list_;

  std::unique_ptr<OobeDisplayChooser> oobe_display_chooser_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_OOBE_UI_H_
