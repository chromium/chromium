# Getting started with User Education

How-to for Chrome and other platforms.

* [Desktop Chrome](/chrome/browser/ui/user_education/README.md)

Currently the only supported platform is Desktop Chrome. See the
[section](#Adding-User-Education-to-your-application) below to learn how
to extend User Education to another platform.


# Adding User Education to your application

There are a number of virtual methods that must be implemented before you can
use these User Education libraries in a new application, mostly centered around
localization, accelerators, and global input focus.

Fortunately for Chromium developers, the browser already has the necessary
support built in for Views, WebUI, and Mac-native context menus. You may refer
to the following locations for an example that could be extended to other
platforms such as ChromeOS:
  * [UserEducationService](
    /chrome/browser/ui/user_education/user_education_service.h) - sets up the
    various registries and `TutorialService`.
  * [BrowserView](/chrome/browser/ui/views/frame/browser_view.cc#831) - sets up
    the `FeaturePromoController`.
  * [browser_user_education_service](
    /chrome/browser/ui/views/user_education/browser_user_education_service.h) -
    registers Chrome-specific IPH and Tutorials.
  * Concrete implementations of abstract User Education base classes can be
    found in [c/b/ui/user_education](/chrome/browser/ui/user_education/) and
    [c/b/ui/views/user_education](/chrome/browser/ui/views/user_education/).
