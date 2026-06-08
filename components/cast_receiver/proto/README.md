# Exo Input Protos

This directory contains the protocol buffer definitions used for sending/receiving Exo input events in the Cast Mirroring Receiver.

## Source of Truth

The **source of truth** for these proto files is the Google-internal `google3` repository:
*   `google3/wireless/android/pixel/exo/proto/`
*   `google3/wireless/android/pixel/exo/services/`

Do **NOT** edit the `.proto` files in this directory directly. Any manual changes will be overwritten during the next synchronization.

## How to Sync

Protos in this directory should be synced manually from their google3 source of truth when updates are needed. Ensure the import paths and packages are adjusted to compile cleanly in Chromium.

