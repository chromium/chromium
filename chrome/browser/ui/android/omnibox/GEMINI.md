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
  - **Coordinator**: The component's public API. It handles creation, lifecycle, and external integration.
  - **Mediator**: Contains the component's business logic. It handles events and triggers state updates. The Mediator **must only** communicate with the View by updating the `PropertyModel`. It must not retain direct references to View objects.
  - **ViewBinder**: A stateless component that translates changes in the `PropertyModel` to the View. This is the **only** class that is permitted to manipulate View properties at runtime.
  - **View**: Android `View` components that hold layout references. They should host very little logic, if any.

## Coding

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
- **Method Signatures**: Avoid creating constructors or methods that accept too many boolean parameters, as this degrades readability.
- **Complexity & Early Returns**: Prefer early return statements over deeply nested conditional statements. Keep the cyclomatic complexity of methods low.
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
- **Strict Mockito Stubs**: All new tests **must** (and existing tests ideally **should**) use strict Mockito stubbing to prevent aggregating dead stubbed code:
  ```java
  @Rule
  public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
  ```
- **Lenient Stubbing (`lenient()`)**:
  - `lenient()` calls should be used in `@Before` / `@BeforeClass` (or shared setup helpers) to configure commonly used mocks.
  - `lenient()` calls are **not allowed** inside `@Test` methods.
  - `lenient()` should be used sparingly—only to address mock calls that are commonly executed and impact a significant number of test cases.
- **Mocking and Spying with Annotations**: Declare mocks (`@Mock`), spies (`@Spy`), and argument captors (`@Captor`) as class fields using Mockito annotations. Calls to `mock()` and `spy()` are highly discouraged / banned unless there is no other way to get something done. `@Mock` and `@Spy` are preferred instead. Similarly, avoid calling `ArgumentCaptor.forClass(...)` inline.
- **Annotation Placement**: Field-level test annotations (`@Rule`, `@Mock`, `@Spy`, `@Captor`) must precede access modifiers (e.g., `@Mock private Foo mFoo;`). `@Rule` fields must be declared `public final`.
- **Naming Conventions**: To clearly distinguish unit tests from integration/instrumentation/render tests, unit test files must be named `*UnitTest.java` (e.g., AutocompleteMediatorUnitTest).
- **Test Length**: Unit tests should be kept concise.
  - The ideal test case is **10 ± 5 lines of code**.
  - Test cases longer than **30 lines of code** are strongly discouraged.
- **Isolating Setup Logic**: Isolate complex initialization and mock configurations inside helper methods or `@Before` setup blocks. The test method itself should focus solely on setting up the specific scenario, triggering the target behavior, and asserting the expected outcomes in a few clear lines.
