# Account Selection Android Feature

This folder contains the internal parts of the Account Selection component.
Files, classes and methods defined in here are not meant to be used outside of
this package.

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


A typical request to the Account Selection component API that intends to change
the visible appearance involves all component parts. For example if the API
caller requests to show new accounts:

1. The API caller asks the controller to show a set of `accounts`.
    1. The controller prepares the `accounts` for display.
    2. The controller writes the prepared accounts into the [model](#Model).
2. The MCP picks up model changes.
    1. The MCP identifies that the `SHEET_ITEMS` property was changed.
    2. The MCP uses a ViewBinder to bind each changed account to the
       corresponding view (e.g. a TextView inside a RecyclerView).
3. The view renders the changed account list.
    1. The view may apply style, and event handlers for click events.


## Model

The model holds state and event listeners connected to the view. An MCP
automatically notifies listener about any change made to a property. To automate
this Observer structure, the model is a `ListModel` as defined in
`//src/ui/android/java/src/org/chromium/ui/modelutil/`. The items in this model
are themselves of type `PropertyModel`. The properties for these are located in
the static `AccountSelectionProperties` class.

The items in the list can be of three types:

 * **HEADER**: There is only one of these and it represents the header of the
   view. Contains whether this is for a single or multiple accounts, and the URL
   for the RP.
 * **ACCOUNT**: One for each account. It contains the account data, favicon, and
   a click listener.
 * **CONTINUE_BUTTON**: There is only one when we want to show a confirm button.

## Controller

The controller of this model implements the AccountSelectionComponent interface
as defined in `public/` and contains all logic. The controller consists of two
parts:

  * **AccountSelectionCoordinator** which implements the public interface and
    creates all parts of the component (model, mediator, view, etc.) and links
    them using MCPs.
  * **AccountSelectionMediator** which handles request to the component API and
    changes the model accordingly. Interactions with the view are typically
    handled here and either affect the model or notify callers of the component
    API. This is also responsible for observing the bottom sheet state and
    notify callers on its dismissal.


## View

We use a simple `LinearLayout` as the top-level view for this component which
contains the list view for sheet items. This view is them displayed inside the
bottom sheet via `AccountSelectionBottomSheetContent`. The rest of the logic is
split in two parts:

 * **AccountSelectionViewBinder** which maps model changes to the view. This is
   mainly used by `SimpleRecyclerViewAdapter` and is responsible to bind changes
   to the items in the model list to the RecyclerView inside the bottom sheet.
 * **AccountSelectionBottomSheetContent** This is a simple container that
   implements `BottomSheetContent` interface and facilitates display of our view
   inside the `BottomSheetController`. The bottom sheet controller instance
   itself is controlled by the mediator to create and modify the bottom sheet
   where accounts are displayed.
