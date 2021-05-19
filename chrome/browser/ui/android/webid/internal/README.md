# Account Selection Android Feature

This folder contains the internal parts of the Account Selection component. Files,
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

## Model

The model holds state and event listeners connected to the view. An MCP
automatically notifies listener about any change made to a property. To automate
this Observer structure, the model is a `PropertyModel` as defined in
`//src/ui/android/java/src/org/chromium/ui/modelutil/`. It is built by defining
readable and writable properties and constructing a model with them. The
properties (and a simple factory method for the model) are located in the static
`AccountSelectionProperties` class.

The model contains writable and readable properties. The readable properties are
guaranteed to never change for the lifetime of the Account Selection component:

 * **DISMISS_HANDLER** which is the listener that reacts to the view being
  dismissed. The mediator implements this.

The writable properties change over the course of the components lifetime:

 * **VISIBLE** which will trigger the component to render the bottom sheet or
   hide it, if it was visible.


## Controller

The controller of this model implements the AccountSelectionComponent interface as
defined in `public/` and contains all logic. The controller consists of two parts:

  * **AccountSelectionCoordinator** which implements the public interface and creates all
    parts of the component (model, mediator, view, etc.) and links them using
    MCPs.
  * **AccountSelectionMediator** which handles request to the component API and changes
    the model accordingly. Interactions with the view are typically handled here
    and either affect the model or notify callers of the component API.


## View

This will be added in follow-up patches.
