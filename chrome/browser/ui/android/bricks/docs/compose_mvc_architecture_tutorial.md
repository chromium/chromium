# So, you want to do MVC in Compose...

## Overview
This document outlines the basic implementation of the MVC (Model-View-Controller) framework in Chrome Android when using Jetpack Compose.

In Compose, the traditional Clank MVC structure is simplified:
* **Model**: Represented by a Kotlin immutable `data class` (`UiState`).
* **View**: A stateless `@Composable` function replacing XML layout files and `ViewBinder` classes.
* **Mediator**: Manages state by observing outside signals and updating a `MutableStateFlow`.
* **Coordinator**: Owns the mediator and state flow, and exposes a `@Composable` content entry point instead of returning an Android `View`.

For this example, we’ll be implementing a simple progress bar that updates its track, fill, and status text based on progress events.

## File Structure
The file structure of our component is located in [`../internal/java/src/org/chromium/chrome/browser/bricks/progress/`](../internal/java/src/org/chromium/chrome/browser/bricks/progress/):
* [`ProgressCoordinator.kt`](../internal/java/src/org/chromium/chrome/browser/bricks/progress/ProgressCoordinator.kt)
* [`ProgressMediator.kt`](../internal/java/src/org/chromium/chrome/browser/bricks/progress/ProgressMediator.kt)
* [`ProgressBar.kt`](../internal/java/src/org/chromium/chrome/browser/bricks/progress/ProgressBar.kt)
* [`ProgressUiState.kt`](../internal/java/src/org/chromium/chrome/browser/bricks/progress/ProgressUiState.kt)

## ProgressCoordinator
The class responsible for setting up the component. This should be the only public class in the component's package and is the only class with direct access to the mediator.

Instead of inflating a layout and providing a `getView()` method, the coordinator exposes a composable `Content()` method that can be called by another component, likely the parent, to embed this component anywhere in its hierarchy. It also connects the Composable to the state it needs to render the UI.

```kotlin
/** Coordinator for the [ProgressBar] component. */
class ProgressCoordinator(provider: ProgressProvider) {
  private val mStateFlow = MutableStateFlow(ProgressUiState())
  private val mProvider = provider
  private val mMediator = ProgressMediator(mStateFlow, mProvider)

  /** Composable content for the progress bar. */
  @Composable
  fun Content(modifier: Modifier = Modifier) {
    val state by mStateFlow.collectAsStateWithLifecycle()
    ProgressBar(state = state, modifier = modifier)
  }

  /** Destroys the [ProgressCoordinator] and its dependencies. */
  fun destroy() {
    mMediator.destroy()
  }
}
```

## ProgressMediator
The class that handles signals coming from the outside world (such as data providers, tab observers, or backend services) and converts them into UI state updates. External classes should never interact with the mediator directly.

```kotlin
/** Mediator that manages [ProgressUiState] based on progress provided by a [ProgressProvider]. */
class ProgressMediator(
  uiState: MutableStateFlow<ProgressUiState>,
  provider: ProgressProvider,
) : ProgressProvider.Observer {
  private val mUiState = uiState
  private val mProvider = provider

  init {
    mProvider.addObserver(this)
  }

  override fun onProgressChanged(progress: Float) {
    mUiState.update {
      it.copy(
        progressFraction = progress,
        isRunning = progress > 0f && progress < 1f,
      )
    }
  }

  /** Destroys the mediator and its dependencies. */
  fun destroy() {
    mProvider.removeObserver(this)
  }
}
```

## ProgressBar
The `@Composable` function responsible for rendering the UI. It replaces the XML layout, `ViewBinder` and `ModelChangeProcessor` in legacy MVC.

The Composable function is stateless: it accepts the current `ProgressUiState` alongside an optional `Modifier`, and declaratively emits UI elements.

```kotlin
/** Composable progress bar displaying [ProgressUiState]. */
@Composable
fun ProgressBar(
  state: ProgressUiState,
  modifier: Modifier = Modifier,
) {
  Column(modifier = modifier.padding(16.dp)) {
    // Track
    Box(
      modifier =
        Modifier.fillMaxWidth()
          .height(8.dp)
          .background(
            color = MaterialTheme.colorScheme.secondaryContainer,
            shape = CircleShape,
          )
    ) {
      // Fill
      Box(
        modifier =
          Modifier.fillMaxWidth(state.progressFraction.coerceIn(0f, 1f))
            .fillMaxHeight()
            .background(
              color = MaterialTheme.colorScheme.primary,
              shape = CircleShape,
            )
      )
    }

    Spacer(modifier = Modifier.height(8.dp))

    Text(
      text =
        when {
          state.isRunning -> "Running... ${(state.progressFraction * 100).toInt()}%"
          state.progressFraction >= 1f -> "Done!"
          else -> "Idle"
        }
    )
  }
}
```

## ProgressUiState
The model representing the visual state of the component. It replaces `PropertyModel` and `PropertyKey` constants with a strongly-typed, immutable Kotlin `data class`.

```kotlin
/** State for the progress bar. */
data class ProgressUiState(
  val progressFraction: Float = 0f,
  val isRunning: Boolean = false,
)
```
