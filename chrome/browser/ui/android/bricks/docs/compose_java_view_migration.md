# Adopting Compose per Java View class

A quicker way to adopt Compose that can also benefit us in the short term is to create individual Compose components that replicate the Java APIs of the equivalent widgets. The goal is to limit Kotlin/Compose usage to a single class per widget.

## Migrating MaterialSwitchWithText

For this example, we’ll be implementing a View class that wraps a Compose widget. This class can be used as a drop-in replacement for `MaterialSwitchWithText`. We’ll try to match the existing component’s Java API as closely as possible.

```kotlin
class ComposeMaterialSwitchWithText
@JvmOverloads
constructor(
  context: Context,
  attrs: AttributeSet? = null,
) : AbstractComposeView(context, attrs), Checkable {

  private var mText by mutableStateOf("")
  private var mChecked by mutableStateOf(false)
  private var mEnabled by mutableStateOf(isEnabled)
  private var mListener: CompoundButton.OnCheckedChangeListener? = null
  private val mSwitch by lazy { Switch(context) }

  init {
    if (attrs != null) {
      val typedArray = context.obtainStyledAttributes(attrs, intArrayOf(android.R.attr.text))
      try {
        typedArray.getString(0)?.let { mText = it }
      } finally {
        typedArray.recycle()
      }
    }
  }

  @Composable
  override fun Content() {
    ChromeMaterialTheme {
      ComposeSwitchWithText(
        text = mText,
        checked = mChecked,
        onCheckedChange = { newChecked ->
          if (mEnabled) {
            setChecked(newChecked)
          }
        },
        enabled = mEnabled,
      )
    }
  }

  fun setText(text: CharSequence?) {
    mText = text?.toString() ?: ""
  }

  fun setText(resId: Int) {
    mText = context.getString(resId)
  }

  fun getText(): String = mText

  override fun setChecked(checked: Boolean) {
    if (mChecked != checked) {
      mChecked = checked
      mListener?.onCheckedChanged(mSwitch, mChecked)
    }
  }

  override fun isChecked(): Boolean = mChecked

  override fun toggle() {
    setChecked(!mChecked)
  }

  override fun setEnabled(enabled: Boolean) {
    super.setEnabled(enabled)
    mEnabled = enabled
  }

  fun setOnCheckedChangeListener(listener: CompoundButton.OnCheckedChangeListener?) {
    mListener = listener
  }
}
```

Here are some things to pay attention to in this code snippet:
1. The constructor takes a `Context` and an `AttributeSet`. This lets us use this class in XML layouts just as we would any other Java View class.
2. The class extends `AbstractComposeView`. This is the base class for custom Views implemented using Compose.
3. We override `AbstractComposeView`’s `Content()` method and return the Composable that will display this widget.
4. The rest of the class is implemented in a way that can be dropped into any layout that previously used `MaterialSwitchWithText`, e.g. `setText` methods and the `Checkable` implementation.

Now that we have our View-wrapping-Compose-content ready, it’s time to use it! Our new View can be inserted in a Views layout just like any other:

```xml
<LinearLayout
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:orientation="vertical"
    android:padding="16dp">

    <!-- ... -->

    <TextView
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:paddingTop="16dp"
        android:paddingBottom="8dp"
        android:text="@string/bricks_java_page_section_compose"
        android:textAppearance="@style/TextAppearance.Headline" />

    <org.chromium.chrome.browser.bricks.switches.ComposeMaterialSwitchWithText
        android:id="@+id/compose_switch_text"
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:text="@string/bricks_java_switch_enabled_on" />

    <!-- ... -->

</LinearLayout>
```

You can see [`ComposeMaterialSwitchWithText`](../internal/java/src/org/chromium/chrome/browser/bricks/switches/ComposeMaterialSwitchWithText.kt) in action in [`bricks_java_view.xml`](../internal/java/res/layout/bricks_java_view.xml) and [`BricksJavaCoordinator.java`](../internal/java/src/org/chromium/chrome/browser/bricks/BricksJavaCoordinator.java).
