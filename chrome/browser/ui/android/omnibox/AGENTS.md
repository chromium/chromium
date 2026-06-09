# Omnibox Developer & Agent Guidelines

This document outlines the architectural patterns, coding standards, and testing guidelines for the Omnibox component in Chrome on Android.

## Architecture

### Subfolder Responsibilities
The Omnibox Java code resides under `chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/` and is divided into the following subfolders:
*   **`suggestions`**: Manages the list of autocomplete suggestions displayed in the omnibox dropdown. This includes processors for different suggestion types (e.g. search, URL, carousel, tail suggestions), the recycler view container, and visual styles for dropdown elements.
*   **`status`**: Implements the status view at the left side of the omnibox (e.g., security indicators, search engine logos, and page controls/IPH indicators).
*   **`styles`**: Contains common styling resources, resource providers, drawing utilities, and image loaders shared across Omnibox views.
*   **`geo`**: Handles geographic location tracking and generation of location headers for search requests.
*   **`voice`**: Provides voice recognition handler and helper utilities to support voice search from the omnibox.
*   **`fusebox`**: Contains implementation for the experimental Fusebox/refinement UI inside the omnibox flow.

### Key Architectural Guidelines
*   **SearchEngineService Role**: Serves as an asset and string provider, handling default search engine details and related icons/strings.
*   **Strict Responsibility Boundaries**: Components must strictly adhere to their designated domain and responsibilities:
    *   The `suggestions` UI stack must focus strictly on layout and rendering. It **must not** attempt to understand or manage the backend mechanics of how Autocomplete works.
    *   The voice recognition handler (e.g., `VoiceRecognitionHandler`) should not formulate URLs.
    *   The status view (e.g., `StatusView`) should not construct template URL icon resources.
*   **Clank MVC Principles**:
    All Omnibox UI modules must strictly follow Clank MVC conventions:
    *   **Coordinator**: The component's public API. It handles creation, lifecycle, and external integration.
    *   **Mediator**: Contains the component's business logic. It handles events and triggers state updates. The Mediator **must only** communicate with the View by updating the `PropertyModel`. It must not retain direct references to View objects.
    *   **ViewBinder**: A stateless component that translates changes in the `PropertyModel` to the View. This is the **only** class that is permitted to manipulate View properties at runtime.
    *   **View**: Android `View` components that hold layout references. They should host very little logic, if any.

## Coding

*   **Listener Cleanup**: Always remove listeners and observers in the component's `destroy()` method to prevent memory leaks.
*   **Destruction Propagation**: A parent component `X` **must** implement a `destroy()` method if any of the subcomponents it owns implements a `destroy()` method. The parent's `destroy()` method must clean up and invoke `destroy()` on all its children.
*   **View Inflation**: Prefer using `AsyncViewInflation` where possible to keep the Main Thread free and reduce startup latency.
*   **Imports**: Use `import` statements whenever possible instead of using fully qualified class names within the code.
*   **Reuse & Pre-research**: Research relevant existing libraries, utilities, and methods before implementing something new. Follow existing patterns in the codebase when applicable.
*   **Constants over Magic Numbers**: Do not create or use magic numbers directly in the code. Define and use descriptive constants instead.
*   **Method Signatures**: Avoid creating constructors or methods that accept too many boolean parameters, as this degrades readability.
*   **Complexity & Early Returns**: Prefer early return statements over deeply nested conditional statements. Keep the cyclomatic complexity of methods low.
*   **Placement**: Ensure logic is implemented in the correct architectural location as early as possible in the flow.
*   **Reusability**: Structure components and logic to be reusable where applicable.

## Testing

*   **Mocking with Annotations**: All mock objects (`@Mock`) and argument captors (`@Captor`) must be created using Mockito annotations. Manual mock creation using `Mockito.mock(...)` or `ArgumentCaptor.forClass(...)` is discouraged.
*   **Naming Conventions**: To clearly distinguish unit tests from integration/instrumentation/render tests, unit test files must be named `*UnitTest.java` (e.g., AutocompleteMediatorUnitTest).
*   **Test Length**: Unit tests should be kept concise.
    *   The ideal test case is **10 ± 5 lines of code**.
    *   Test cases longer than **30 lines of code** are strongly discouraged.
*   **Isolating Setup Logic**: Isolate complex initialization and mock configurations inside helper methods or `@Before` setup blocks. The test method itself should focus solely on setting up the specific scenario, triggering the target behavior, and asserting the expected outcomes in a few clear lines.
