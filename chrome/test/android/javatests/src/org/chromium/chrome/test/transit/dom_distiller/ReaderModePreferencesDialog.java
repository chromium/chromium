package org.chromium.chrome.test.transit.dom_distiller;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.transit.ViewElement.scopedViewElement;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.action.GeneralClickAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Tap;
import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.test.transit.CarryOn;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.MenuUtils;

/**
 * The Reader Mode preferences dialog, from where font, font size and background color can be set.
 *
 * <p>TODO(crbug.com/350074837): Turn this into a Facility when CctPageStation exists.
 */
public class ReaderModePreferencesDialog extends CarryOn {

    public static final ViewElement TEXT_COLOR_LIGHT = scopedViewElement(withText("Light"));
    public static final ViewElement TEXT_COLOR_DARK = scopedViewElement(withText("Dark"));
    public static final ViewElement TEXT_COLOR_SEPIA = scopedViewElement(withText("Sepia"));
    public static final ViewElement FONT_FAMILY = scopedViewElement(withId(R.id.font_family));
    public static final ViewElement FONT_SIZE_SLIDER = scopedViewElement(withId(R.id.font_size));

    @Override
    public void declareElements(Elements.Builder elements) {
        /*
        DecorView
        ╰── LinearLayout
            ╰── FrameLayout
                ╰── @id/action_bar_root | FitWindowsFrameLayout
                    ╰── @id/content | ContentFrameLayout
                        ╰── @id/parentPanel | AlertDialogLayout
                            ╰── @id/customPanel | FrameLayout
                                ╰── @id/custom | FrameLayout
                                    ╰── DistilledPagePrefsView
                                        ├── @id/radio_button_group | RadioGroup
                                        │   ├── "Light" | @id/light_mode | MaterialRadioButton
                                        │   ├── "Dark" | @id/dark_mode | MaterialRadioButton
                                        │   ╰── "Sepia" | @id/sepia_mode | MaterialRadioButton
                                        ├── @id/font_family | AppCompatSpinner
                                        │   ╰── "Sans Serif" | @id/text1 | MaterialTextView
                                        ╰── LinearLayout
                                            ├── "200%" | @id/font_size_percentage | MaterialTextView
                                            ├── "A" | MaterialTextView
                                            ├── @id/font_size | AppCompatSeekBar
                                            ╰── "A" | MaterialTextView
        */
        elements.declareView(TEXT_COLOR_DARK);
        elements.declareView(TEXT_COLOR_SEPIA);
        elements.declareView(TEXT_COLOR_LIGHT);
        elements.declareView(FONT_FAMILY);
        elements.declareView(FONT_SIZE_SLIDER);
    }

    public void pickColorLight(Condition condition) {
        Condition.runAndWaitFor(() -> TEXT_COLOR_LIGHT.perform(click()), condition);
    }

    public void pickColorDark(Condition condition) {
        Condition.runAndWaitFor(() -> TEXT_COLOR_DARK.perform(click()), condition);
    }

    public void pickColorSepia(Condition condition) {
        Condition.runAndWaitFor(() -> TEXT_COLOR_SEPIA.perform(click()), condition);
    }

    public void setFontSizeSliderToMin(Condition condition) {
        // Min is 50% font size.
        Condition.runAndWaitFor(
                () ->
                        FONT_SIZE_SLIDER.perform(
                                new GeneralClickAction(
                                        Tap.SINGLE, GeneralLocation.CENTER_LEFT, Press.FINGER)),
                condition);
    }

    public void setFontSizeSliderToMax(Condition condition) {
        Condition.runAndWaitFor(
                () ->
                        FONT_SIZE_SLIDER.perform(
                                new GeneralClickAction(
                                        Tap.SINGLE, GeneralLocation.CENTER_RIGHT, Press.FINGER)),
                condition);
    }

    public void pressBackToClose() {
        drop(Espresso::pressBack);
    }

    public static ReaderModePreferencesDialog open(ChromeActivity<?> activity) {
        return CarryOn.pickUp(
                new ReaderModePreferencesDialog(),
                () ->
                        MenuUtils.invokeCustomMenuActionSync(
                                InstrumentationRegistry.getInstrumentation(),
                                activity,
                                R.id.reader_mode_prefs_id));
    }
}
