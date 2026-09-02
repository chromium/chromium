# Identity Manager Objective-C Bridge

Freshness: 2026-09-02

## Overview
This package provides an Objective-C adapter bridge for [IdentityManager](../README.md) (`//components/signin/public/identity_manager`). It allows iOS Objective-C and Objective-C++ consumers (e.g., in `//ios/chrome`) to observe identity lifecycle events, account state changes, refresh token updates, and Gaia cookie jar modifications via standard Objective-C protocols and weak delegates without directly subclassing C++ observer interfaces.

## Interface
- [`IdentityManagerObserving`](./identity_manager_observer_bridge.h): Objective-C protocol defining optional delegate callback methods corresponding to `signin::IdentityManager::Observer` events (e.g., primary account changes, token updates/removals, cookie jar modifications, extended account info updates, and shutdown).
- [`signin::IdentityManagerObserverBridge`](./identity_manager_observer_bridge.h): C++ bridge class implementing `signin::IdentityManager::Observer`. Observes a `signin::IdentityManager` instance via `base::ScopedObservation` and forwards dispatched events to an `id<IdentityManagerObserving>` target.

## Invariants
- **Weak Delegate Retention**: The bridge holds a `__weak` reference to `id<IdentityManagerObserving>`. If the delegate is deallocated, ARC zeroes the reference and subsequent event dispatches are safely skipped.
- **Optional Delegate Selectors**: Delegate callbacks are optional; the bridge queries `respondsToSelector:` before invoking any callback on the target.
- **Re-entrant Shutdown Safety**: During `OnIdentityManagerShutdown`, the bridge resets its observation and clears its `IdentityManager` pointer before notifying the delegate via `identityManagerDidShutdown:`, safely tolerating immediate deallocation of the bridge by its owner inside the callback.

## Side Effects
- **Observer Registration**: Registers as an observer on `signin::IdentityManager` upon creation and unregisters on destruction or during `OnIdentityManagerShutdown`.
- **Objective-C Event Dispatch**: Invokes corresponding delegate methods on the weak `IdentityManagerObserving` target in response to C++ `IdentityManager::Observer` notifications.
- **No State Mutation**: Does not mutate preferences, storage, or network state; acts strictly as a read-only event forwarder.

## Verification
- **Build**: `autoninja -C out/ios_sim components/signin/public/identity_manager/objc` _(Requires `target_os = "ios"` in args.gn)_
- **Test**: `out/ios_sim/components_unittests --gtest_filter="IdentityManagerObserverBridgeTest.*"`
