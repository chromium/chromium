# Publisher

This directory contains the App Service publisher classes for web apps (Desktop PWAs and shortcut apps).

App Service [publishers](../../../../chrome/browser/apps/app_service/public/app_publisher.h) keep the App Service [updates](../../../../components/services/app_service/public/cpp/app_update.h) with the set of installed apps, and implement commands such as launching.

For Ash, Linux, Mac and Windows, the publisher is [WebApps](web_apps.h). (This is currently also used to support the chrome://apps page in the Lacros browser.)

In Ash with the Lacros flags enabled, this publisher only manages system web apps.

For Lacros, we have a proxy publisher [WebAppsCrosapi](../../apps/app_service/publishers/web_apps_crosapi.h) in the Ash process, that communicates over [mojom](../../../../chromeos/crosapi/mojom/app_service.mojom) with the Lacros [LacrosWebAppsController](lacros_web_apps_controller.h). (The web app registry for non-system web apps is in the Lacros process, while the App Service controlling the Chrome OS launcher, shelf, etc. is in the Ash process.)

# WebAppPublisherHelper

Each of the web app publisher classes delegate the majority of their functionality to [WebAppPublisherHelper](web_app_publisher_helper.h).

This class observes updates to the installed set of web apps, and forwards these updates to the publisher that owns it, so the updates can then be forwarded to the App Service. 
