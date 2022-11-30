# Fast Checkout Android Feature

This folder contains the internal parts of the Fast Checkout component. Files,
classes and methods defined in here are not meant to be used outside of this
package.

This document provides a brief overview of the architecture.

[TOC]

## Component structure

This component follows the typical
[MVC](https://en.wikipedia.org/wiki/Model%E2%80%93view%E2%80%93controller)
structure that is widely used in Chrome on Android. The MVC structures separates
logic from representation:

 * The [controller](#Controller) creates and connects all subcomponents and
   performs logic that affects the visual appearance of the component.
 * The [model](#Model) keeps all state that affects the visual appearance of the
   component. Changes made to it are automatically propagated to the view by a
   Model-Change-Processor (MCP) as defined in
    `//src/ui/android/java/src/org/chromium/ui/modelutil/`
 * The [view](#View) is the representation of the component. It enforces styles
   and is mostly called by a view binder to set mutable properties.


A typical request to the Fast Checkout component API that intends to change the
visible appearance involves all component parts. For example if the API caller
requests to show new options:

1. The API caller asks the controller to show a set of
   `autofill profiles` and `credit cards`.
    1. The controller prepares and filters the options for display.
    2. The controller writes the prepared options into the [model](#Model).
2. The MCP picks up model changes.
    1. The MCP identifies that the `AUTOFILL_PROFILE_LIST` and `CREDIT_CARD_LIST`
       properties were changed.
    2. The MCP uses a ViewBinder to bind each changed option to the
       corresponding view (e.g. a TextView inside a LinearLayout).
3. The view renders the corresponding UI.
    1. The view may apply style, RTL settings and event handlers for click events.
