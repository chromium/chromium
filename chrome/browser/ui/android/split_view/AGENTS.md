# Android Split View

For the full engineering design, diagrams, persistence schema, and rationale, see [README.md](README.md).

## Architectural layers

Keep Split View separated into three layers with strict responsibilities:

1. **[Data Layer](README.md#131-data-layer)** (`//chrome/browser/tab*`, `//components/tabs/`): Serves as the single source of truth for Split View state and persistence, and provides tab APIs to the UI Layer and Feature Layer.
2. **[UI Layer](README.md#132-ui-layer)** (`CompositorViewHolder`, `StaticLayout`, `StaticTabSceneLayer`, browser UI): Renders state provided by the Data Layer and forwards user actions to the Feature Layer.
3. **[Feature Layer](README.md#133-feature-layer)** (`//chrome/browser/ui/android/split_view/`): Centralizes Split View business logic (entry and exit flows, resizing, responsive layout adaptation) inside a dedicated Android module instead of scattering CUJ logic across infrastructure components.

## Core invariants

### `foregroundTabs` versus `currentTab`

* `foregroundTabs` (`TabModelSelector#getForegroundTabs()`, `TabModel#getForegroundTabsSupplier()`): Contains the ordered list of all visible tabs (1 tab in single-tab mode, 2 tabs in Split View) and always includes the active tab. Use `foregroundTabs` for components that render or observe Split View visibility.
* `currentTab` (`TabModelSelector#getCurrentTab()`, `TabModel#getCurrentTabSupplier()`): Identifies the single active tab (at most 1). Preserve `currentTab` semantics so single-tab-scoped features (Omnibox, back and forward navigation, tab-scoped side panels) continue operating on the active tab.

### Single-compositor in-place rendering

* Render Split View inside the Activity window's primary `CompositorView` and `StaticTabSceneLayer` tree rather than adopting `ThinWebView`. Using `ThinWebView` creates an orphaned `WindowAndroid` and breaks `InputOnViz`, fluid resize synchronization, browser controls integration, and `UnownedUserData` bindings (see [CompositorView vs. ThinWebView](README.md#appendix-compositorview-vs-thinwebview)).
* Refactor `CompositorViewHolder`, `StaticLayout`, and `StaticTabSceneLayer` in place rather than forking those classes ([Rollout plan](README.md#31-rollout-plan)):
  * **Phase 1**: Generalize single-tab data structures and APIs to handle 1 or 2 foreground tabs as a no-op refactoring while preserving single-tab behavior.
  * **Phase 2**: Add multi-tab rendering support on top of Phase 1.

### Centralized feature-flag guarding

* Gate Split View with `ChromeFeatureList.sEnableAndroidSplitView` (`enable-android-split-view`, native `chrome::android::kEnableAndroidSplitView`).
* Centralize flag checks inside `SplitViewFactory` so disabled builds receive no concrete Feature Layer implementations and avoid scattered `if/else` flag checks across the Data Layer and UI Layer.
* **Persistence rollback exception**: Guard the deserialization read path in [Tab Persistence](README.md#13122-tab-persistence) (`TabStateStore`, `TabPersistentStoreImpl`, `SplitTabVisualDataStore`) with the feature flag so turning off the flag rolls back cleanly without corrupting restored tab state.
