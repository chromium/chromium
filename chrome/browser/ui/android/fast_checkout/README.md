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
