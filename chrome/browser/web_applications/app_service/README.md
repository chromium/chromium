# Publisher

This directory contains the App Service publisher classes for web apps (Desktop PWAs and shortcut apps).

App Service [publishers](../../../../chrome/browser/apps/app_service/public/app_publisher.h) keep the App Service [updates](../../../../components/services/app_service/public/cpp/app_update.h) with the set of installed apps, and implement commands such as launching.

For Ash, Linux, Mac and Windows, the publisher is [WebApps](web_apps.h).

# WebAppPublisherHelper

Each of the web app publisher classes delegate the majority of their functionality to [WebAppPublisherHelper](web_app_publisher_helper.h).

This class observes updates to the installed set of web apps, and forwards these updates to the publisher that owns it, so the updates can then be forwarded to the App Service.
