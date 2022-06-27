# Fast Checkout Android Feature

This folder contains the Android UI implementation for the Fast Checkout
feature. Fast Checkout provides users with a trusted surface to authorize
transactions, such as filling their email, address and credit card information
during a checkout flow.

[TOC]

## Use case

This component displays a set of saved addresses and credit cards. The user
selects one of each, which is then filled into the corresponding form while the
user moves along the checkout flow. If the user dismisses the sheet, the
keyboard will be shown instead (i.e. by changing the focus).


## Folder Structure

#### java/

The root folder contains the public interface of this component and data that is
used to fill it with content, e.g. Autofill Profiles, Credit Cards.

Add `chrome/browser/ui/android/fast_checkout:java` as a dependency to use the
interface and classes defined here.

#### java/internal/

Contains the actual implementation. Don't try to use any class defined here
outside of this package. If you need access to any method, consider making it
part of the public interface as defined in `FastCheckoutComponent`.

This folder contains a separate [README](internal/README.md) that explains in
detail how the architecture looks like and how to extend the component further.
