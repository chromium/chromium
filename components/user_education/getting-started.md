# Getting started with User Education

Currently the only supported platform is Desktop Chrome. See the
[section](#Adding-User-Education-to-your-application) below to learn how
to extend User Education to another platform.

The following User Education primitives are available in Chrome:
* [In-product help (IPH)](architecture.md#iph-how-to):
  help dialogs offered by Chrome suggesting useful
  features. These are triggered automatically based on user behavior and give a 
  short value statement and directions to use the feature
* [New badge](architecture.md#new-badge): pop-out label applied to new features'
  entry points to make them more obvious
* [Tutorials](architecture.md#tutorials): step-by-step guided walkthroughs of
  features. User-initiated and more thorough than single-step IPH promotions
* [Open page and highlight](architecture.md#open-page-and-highlight): useful for
  pointing users at a particular settings or other internal page. Opens an
  internal page and shows a help bubble on a particular item.

If you want to display help bubbles on or in a WebUI surface (such
as an internal page), you will need to _instrument_ that page.
[Start here](./webui/README.md) for instructions.

There is common setup for determining when a New Badge or IPH will show. See
[Configuring the Feature Engagement backend](architecture.md#configuring-the-feature-engagement-backend).
You can also learn
[how to test this configuration](architecture.md#testing-feature-engagement-features).

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
    /chrome/browser/ui/views/user_education/browser_user_education_service.cc) -
    registers Chrome-specific IPH and Tutorials.
  * Concrete implementations of abstract User Education base classes can be
    found in [c/b/ui/user_education](/chrome/browser/ui/user_education/) and
    [c/b/ui/views/user_education](/chrome/browser/ui/views/user_education/).
