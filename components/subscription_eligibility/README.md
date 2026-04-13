# Subscription Eligibility Component

This component manages user eligibility for various subscriptions, specifically
focusing on subscription tiers.

## Core Classes

### SubscriptionEligibilityService
The primary interface for this component. It is a `KeyedService` that:
- Provides the current tier via `GetAiSubscriptionTier()`.
- Allows observers to listen for updates to the subscription tier.
- Integrates with `PrefService` to persist and monitor tier information.
