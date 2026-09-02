# Omnibox Developer & Agent Guidelines

This document outlines the architectural patterns, coding standards, and testing guidelines for the Omnibox component in Chrome on Android.

## Maintaining These Guidelines

Agents working within or modifying the Omnibox codebase are expected to actively maintain this file:

- **Keep Guidance Accurate & Current**: When any instruction, architectural pattern, or code reference in this document becomes outdated—either as a direct result of a change or in relation to it—update this file accordingly.
- **Capture New General Rules**: When discovering, clarifying, or establishing general architectural rules, design patterns, or constraints (e.g., interface immutability, component ownership, lifecycle expectations), proactively amend this document to preserve the knowledge for future agents and developers.

## Architecture

### Subfolder Responsibilities

The Omnibox Java code resides under `chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/` and is divided into the following subfolders:

- **`suggestions`**: Manages the list of autocomplete suggestions displayed in the omnibox dropdown. This includes processors for different suggestion types (e.g. search, URL, carousel, tail suggestions), the recycler view container, and visual styles for dropdown elements.
- **`status`**: Implements the status view at the left side of the omnibox (e.g., security indicators, search engine logos, and page controls/IPH indicators).
- **`styles`**: Contains common styling resources, resource providers, drawing utilities, and image loaders shared across Omnibox views.
- **`geo`**: Handles geographic location tracking and generation of location headers for search requests.
- **`voice`**: Provides voice recognition handler and helper utilities to support voice search from the omnibox.
- **`fusebox`**: Contains implementation for the experimental Fusebox/refinement UI inside the omnibox flow.

### Key Architectural Guidelines

- **SearchEngineService Role**: Serves as an asset and string provider, handling default search engine details and related icons/strings.
- **Strict Responsibility Boundaries**: Components must strictly adhere to their designated domain and responsibilities:
  - The `suggestions` UI stack must focus strictly on layout and rendering. It **must not** attempt to understand or manage the backend mechanics of how Autocomplete works.
  - The voice recognition handler (e.g., `VoiceRecognitionHandler`) should not formulate URLs.
  - The status view (e.g., `StatusView`) should not construct template URL icon resources.
- **Clank MVC Principles**:
  All Omnibox UI modules must strictly follow Clank MVC conventions:
  - **Coordinator**: The component's public API. It handles creation, lifecycle, and external integration. Like Mediators, Coordinators should avoid direct manipulation of views.
  - **Mediator**: Contains the component's business logic. It handles events and triggers state updates. The Mediator **must only** communicate with the View by updating the `PropertyModel`. It must not retain direct references to View objects or manipulate them directly.
  - **ViewBinder**: A stateless component that translates changes in the `PropertyModel` to the View. This is the **only** class that is permitted to manipulate View properties at runtime.
  - **View**: Android `View` components that hold layout references. They should host very little logic, if any.
  - **Property-Driven View Updates**: Coordinators and Mediators **must avoid manipulating views directly**. If the component is a proper MVC component, and a change can be represented using properties, it **must** be represented via properties in the `PropertyModel`. In almost all cases (including context menu content, visibility, click handlers, styling, and text state), properties should be used. Exceptions may arise only when it is impossible to capture and agree on a discrete state (e.g. transient actions like `requestFocus()`).

## Coding

### Model Properties (`*Properties.java`) & ViewBinders

- **Property-Driven State Representation**: If a component is a proper MVC component and a change can be represented using properties, it **must** be captured as a property in `PropertyModel`. Coordinators and Mediators should avoid manipulating views directly. In virtually all cases (including context menu content, text state, styling, listeners, and visibility), properties should be used; exceptions arise only when it is impossible to capture and agree on a state (e.g. `requestFocus()`).
- **Alphabetical Sorting**: Properties listed in `*Properties.java` files must be sorted alphabetically for easier lookup (both within field declarations and in `ALL_KEYS` / `ALL_UNIQUE_KEYS` arrays).
- **Semantic Grouping & Naming**: Properties listed in `*Properties.java` files must be grouped semantically by prefix (e.g. `BTN_ADD_VISIBLE`, `BTN_ADD_ENABLED`, `BTN_ADD_CALLBACK`) so alphabetical sorting naturally groups related properties together.
- **ViewBinder Order Consistency**: `ViewBinder` binding logic (`bind(...)` method's `if/else if` chain or dispatch logic) must follow the exact same order as `*Properties.java` for all new code.
- **`@IntDef` Properties**: Properties representing an `@IntDef` **MUST** use `WritableIntDefPropertyKey<T>` or `ReadableIntDefPropertyKey<T>` (typed with the `@IntDef` annotation interface) rather than generic `WritableIntPropertyKey` / `ReadableIntPropertyKey` for clarity, documentation, and compile-time safety.
- **Prefer `ReadablePropertyKey`s**: Where applicable (such as fixed callbacks, listeners, immutable values, or delegates set only during model instantiation and never mutated afterward), `ReadablePropertyKey`s (`ReadableObjectPropertyKey`, `ReadableIntDefPropertyKey`, `ReadableBooleanPropertyKey`, etc.) should be preferred over `WritablePropertyKey`s.

### General Guidelines

- **Listener Cleanup**: Always remove listeners and observers in the component's `destroy()` method to prevent memory leaks.
- **Destruction Propagation**: A parent component `X` **must** implement a `destroy()` method if any of the subcomponents it owns implements a `destroy()` method. The parent's `destroy()` method must clean up and invoke `destroy()` on all its children.
- **View Inflation**: Prefer using `AsyncViewInflation` where possible to keep the Main Thread free and reduce startup latency.
- **Imports**: Use `import` statements whenever possible instead of using fully qualified class names within the code.
- **Javadoc & Method Contracts**:
  - Keep Javadoc comments updated to reflect code changes. Javadoc must accurately capture what the method does and its proper contract (parameters, return values, side effects, and expectations).
  - When updating classes, always read the top-level class comment to catch any critical context, invariants, or restrictions (what is / what is not allowed).
- **Reuse & Pre-research**: Research relevant existing libraries, utilities, and methods before implementing something new. Follow existing patterns in the codebase when applicable.
- **Resource & Type Annotations**: Always annotate integer resource IDs and typed values with appropriate AndroidX annotations (e.g., `@ColorInt`, `@ColorRes`, `@DrawableRes`, `@StringRes`, `@Px`).
- **Constants over Magic Numbers**: Do not create or use magic numbers directly in the code. Define and use descriptive constants instead.
- **Method Signatures & Parameter Comments**:
  - Avoid creating constructors or methods that accept too many boolean parameters, as this degrades readability.
  - **Boolean Parameter Annotations**: Call-site boolean literals must be documented with a `/* paramName= */` comment unless the parameter's meaning is unmistakably clear from the method name (e.g., `setVisible(true)` is fine, but `open(view, /* animated= */ true)` is not). Note that ErrorProne strictly verifies that `paramName` matches the exact formal parameter name in the method declaration (`[ParameterName]`). Always check the target method declaration, or use `/* comment */` without `=` if not matching.
  - **Repeated Plain-Old-Data (POD) Parameters**: Repeated primitive / POD parameters (e.g., consecutive `int`, `long`, `float`, `boolean` values) unconditionally must be documented at call sites with `/* paramName= */` comments unless the parameter order is self-evident from the method name (e.g., `new Rect(...)` is fine, but `MotionEvent.obtain(/* downTime= */ 0, /* eventTime= */ 0, /* action= */ ACTION_DOWN, /* x= */ 0, /* y= */ 0, /* metaState= */ 0)` is not). Exact formal parameter names are required by ErrorProne.
- **Complexity & Early Returns**: Prefer early return statements over deeply nested conditional statements. Keep the cyclomatic complexity of methods low.
- **Avoid `instanceof` Checks**: Avoid using `instanceof` and explicit downcasting. `instanceof` is typically a code smell indicating that concrete implementation details are being shoehorned into code that should be properly abstracted. Prefer polymorphism, interface contracts, or delegating behavior directly to the class hierarchy rather than type-checking and branching on concrete types.
- **Placement**: Ensure logic is implemented in the correct architectural location as early as possible in the flow.
- **Reusability**: Structure components and logic to be reusable where applicable.

## Feature Flags

When introducing or modifying Omnibox feature flags:

- **C++ Definitions**:
  - Declare the feature in `components/omnibox/common/omnibox_features.h` (`BASE_DECLARE_FEATURE(kOmniboxFoo);`).
  - Define the feature in `components/omnibox/common/omnibox_features.cc` (`BASE_FEATURE(kOmniboxFoo, DISABLED);`).
  - Expose it to Java by adding `&kOmniboxFoo` to `kFeaturesExposedToJava` in `components/omnibox/common/omnibox_features.cc`. This automatically generates `OmniboxFeatureList.OMNIBOX_FOO`.
- **Java Wrapper**:
  - In `components/omnibox/common/android/java/src/org/chromium/components/omnibox/OmniboxFeatures.java`, define a `CachedFlag` via `newFlag(OmniboxFeatureList.OMNIBOX_FOO, FeatureState.DISABLED)`.
  - Expose a public accessor `isFooEnabled()`, and if needed for Robolectric unit tests, a `@Nullable Boolean` test override setter (`setFooForTesting(@Nullable Boolean)`).
- **chrome://flags Exposure**:
  - Add name and description constants to `chrome/browser/flag_descriptions.h` (`kOmniboxFooName`, `kOmniboxFooDescription`).
  - Add the entry under `#if BUILDFLAG(IS_ANDROID)` in `chrome/browser/about_flags.cc` using `FEATURE_VALUE_TYPE(omnibox::kOmniboxFoo)`.

## Testing

- **Test File Registration**: When creating a new unit test file (e.g. `FooUnitTest.java`), always register it in `chrome/browser/ui/android/omnibox/BUILD.gn` under the `robolectric_tests` `sources` list so `an -t` and GN correctly resolve the build target.
- **Naming Conventions**: To clearly distinguish unit tests from integration/instrumentation/render tests, unit test files must be named `*UnitTest.java` (e.g., AutocompleteMediatorUnitTest).
- **Drop `@Config(manifest = Config.NONE)`**: Do not include `@Config(manifest = Config.NONE)` (or `@Config(manifest = NONE)`).
  - Omnibox unit tests are executed under `chrome_junit_tests`, which compiles Chrome's Android resources and merged manifest into a `resource_apk` archive (`chrome_junit_tests.robo.ap_`), supplied to Robolectric via `android_resource_apk` in `test_config.properties`.
  - When `android_resource_apk` is present, Robolectric's build-system loader (`DefaultManifestFactory`) loads the manifest directly from the `resource_apk` and **explicitly ignores** `@Config(manifest = ...)` annotations.
  - For resource-free suites (e.g. `base_junit_tests`), `manifest = --none` is injected globally into `robolectric.properties` by `local_machine_junit_test_run.py`.
  - In all scenarios, class-level `@Config(manifest = ...)` annotations are completely redundant no-ops and should be omitted from new tests and dropped from existing tests.
- **Avoid Test Size Annotations in Unit Tests**: Do not annotate unit tests (`*UnitTest.java`) with size annotations such as `@SmallTest`, `@MediumTest`, or `@LargeTest`. These annotations are only relevant for on-device instrumentation tests (`*Test.java`) where the runner uses them to enforce timeouts and shard batches. In host-based Robolectric unit tests, they have no effect, carry no meaning, and are purely redundant boilerplate.
- **Test Length**: Unit tests should be kept concise.
  - The ideal test case is **10 ± 5 lines of code**.
  - Test cases longer than **30 lines of code** are strongly discouraged.
- **Isolating Setup Logic**: Isolate complex initialization and mock configurations inside helper methods or `@Before` setup blocks. The test method itself should focus solely on setting up the specific scenario, triggering the target behavior, and asserting the expected outcomes in a few clear lines.
- **Strict Mockito Stubs**: All new tests **must** (and existing tests ideally **should**) use strict Mockito stubbing to prevent aggregating dead stubbed code:
  ```java
  @Rule
  public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
  ```
- **Lenient Stubbing (`lenient()`)**:
  - `lenient()` calls should be used in `@Before` / `@BeforeClass` (or shared setup helpers) to configure commonly used mocks.
  - `lenient()` calls are **not allowed** inside `@Test` methods.
  - `lenient()` should be used sparingly—only to address mock calls that are commonly executed and impact a significant number of test cases.
- **Mocking and Spying with Annotations**:
  - Declare mocks (`@Mock`), spies (`@Spy`), and argument captors (`@Captor`) as class fields using Mockito annotations.
  - Direct runtime calls to `mock()` and `spy()` (as well as `ArgumentCaptor.forClass(...)`) are banned / highly discouraged due to proxy creation overhead, invocation recording, and GC pressure. `@Mock` and `@Spy` field annotations initialized once by `MockitoRule` are preferred instead.
  - Prefer plain Java fakes/stubs (implementing interfaces directly) or lightweight real objects over mocks where feasible.
  - Adoption of `spy()` should be cautious and rare, as `spy()` executes real constructors and intercepts all nested self-invocations.
- **Annotation Placement**: Field-level test annotations (`@Rule`, `@Mock`, `@Spy`, `@Captor`) must precede access modifiers (e.g., `@Mock private Foo mFoo;`). `@Rule` fields must be declared `public final`.
- **Keep Tests Fast & Unwelcome Test Patterns**:
  Heavy Mockito constructs and framework-level reflection introduce measurable execution overhead, high GC pressure, and bytecode transformation penalties. Require cautious and rare adoption of these patterns to keep tests fast:
  - **`mockStatic(...)` and `mockConstruction(...)` (Unwelcome)**:
    - *Problem*: Forces the ByteBuddy agent to dynamically re-instrument loaded classes in memory, requiring global locking, thread-local state tracking, and expensive cleanup.
    - *Alternative*: Refactor code to use dependency injection (passing instances, factories, or `Supplier`s) rather than invoking static methods or un-injected constructors.
  - **`verifyNoMoreInteractions(...)` and `inOrder(...)` (Unwelcome)**:
    - *Problem*:
      - *Performance*: Scans and walks the entire invocation log across all mock instances to reconstruct chronological call sequences.
      - *Pitfall*: `inOrder.verify(...)` does not verify whether an in-order action happened as a result of the immediate preceding test step or was triggered earlier by something else (e.g. during initial setup). See [`ActionButtonViewUnitTest.java#showOnlyFocusButton_selected`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/suggestions/base/ActionButtonViewUnitTest.java;l=68-80;drc=ad765941be2ebfb09c8a0014c89b6d063f863cfa) as a textbook example of this issue.
    - *Alternative*: Prefer state verification (checking output models or property values) or direct, targeted `verify(...)` calls on critical interfaces. The recommended approach is:
      - `verify(mock).method(argument);` (implies `times(1)`)
      - `verify(mock).method(any());` (implies `times(1)`)
      - Avoid bundling explicit `/* times(1) */` / `times(1)` parameters since single invocation is already the default. If verifying actions across distinct test phases, isolate step boundaries using `clearInvocations(mock)`.
  - **`reset(mock)` (Unwelcome)**:
    - *Problem*: Reconstructs internal Mockito handler state, wipes configured stubs, defeats JIT optimizations, and can leak state across test runs.
    - *Alternative*: Use `clearInvocations(mock)` if you only need to discard recorded interactions without resetting stubs, or instantiate fresh mock instances per test method via standard `@Before` or `@Mock` field lifecycle.
  - **Inconsistent `@Config` across Robolectric Tests (Unwelcome)**:
    - *Problem*: Varying `@Config(shadows = ...)` or `@Config(sdk = ...)` forces Chromium's test harness (`local_machine_junit_test_run.py`) to fragment tests into separate process shards, or triggers runtime failures (`"Invalid test batch detected"`) in `BaseRobolectricTestRunner`. Additionally, `@Config(qualifiers = "...")` contributes directly to the shard grouping key in `TestListComputer`, so introducing arbitrary or unique qualifier strings splits tests into separate process buckets.
    - *Alternative*: Use standardized Robolectric configuration across test suites; avoid custom shadows or SDK variants when real Android or POJO classes can be used. Using `@Config(qualifiers = ...)` is acceptable for establishing device/screen configurations, but avoid proliferating too many distinct configs—standardize on and reuse existing common configs where possible, or adjust qualifiers dynamically during test execution (e.g. `RuntimeEnvironment.setQualifiers(...)`).
  - **Java Reflection (`setAccessible(true)` / `ReflectionTestUtils`) (Unwelcome)**:
    - *Problem*: Bypasses encapsulation, breaks JIT escape analysis and method inlining, and produces fragile tests.
    - *Alternative*: Interact with the class under test through existing public contracts or via its `PropertyModel` (the primary intended interface in Clank MVC). If internal state access is unavoidable, provide package-private `@VisibleForTesting` accessors or `getFooForTesting()` / `setFooForTesting()` methods.

