# WebID Android Feature

This folder contains the Android UI implementation for the WebID feature.

[TOC]

## Use case

This component displays a set of accounts. The user selects one which is then
used to complete authentication via WebID mediation based approach.


## Folder Structure

#### java/

The root folder contains the public interface of this component and data that is
used to fill it with content, e.g. Account. This folder also contains the
factory to instantiate the component.

Add `chrome/browser/ui/android/webid/android:public_java` as dependency to use
the interface and classes defined here.

#### java/internal/

Contains the actual implementation. Don't try to use any class defined here
outside of this package. If you need access to any method, consider making it
part of the public interface as defined in `AccountSelectionComponent`

At the moment the implementation is a simple stub that selects the first
account.
