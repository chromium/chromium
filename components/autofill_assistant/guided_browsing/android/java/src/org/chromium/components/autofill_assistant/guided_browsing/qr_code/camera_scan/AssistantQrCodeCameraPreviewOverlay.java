// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.DynamicLayout;
import android.text.Layout.Alignment;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.view.View;

import androidx.core.content.ContextCompat;

import org.chromium.components.autofill_assistant.guided_browsing.R;

/**
 * AssistantQrCodeCameraPreviewOverlay is a mainly transparent layer meant for the camera preview.
 */
public class AssistantQrCodeCameraPreviewOverlay extends View {
    private final int mRectSize;
    private final int mCornerSize;
    private final Paint mCornerPaint;
    private final Paint mMaskPaint;
    private final TextPaint mTextPaint;
    private final int mInstructionTextTopPadding;
    private final int mSecurityTextTopPadding;
    private final DynamicLayout mInstructionTextLayout;
    private final Drawable mSecurityImg;

    private DynamicLayout mSecurityTextLayout;
    private Rect mFramingRect;
    private SpannableStringBuilder mInstructionText;
    private SpannableStringBuilder mSecurityText;

    public AssistantQrCodeCameraPreviewOverlay(Context context) {
        super(context);

        mRectSize = getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_rect_size);
        mCornerSize = getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_corner_size);
        mInstructionTextTopPadding = getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_instruction_text_top_padding);
        mSecurityTextTopPadding = getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_security_text_top_padding);
        updateFramingRect();

        mMaskPaint = new Paint();
        mMaskPaint.setColor(getResources().getColor(R.color.black_alpha_65));

        mCornerPaint = new Paint();
        mCornerPaint.setColor(getResources().getColor(android.R.color.white));
        mCornerPaint.setStyle(Paint.Style.STROKE);
        mCornerPaint.setStrokeWidth(getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_corner_width));

        mTextPaint = new TextPaint();
        mTextPaint.setAntiAlias(true);
        mTextPaint.setColor(getResources().getColor(android.R.color.white));
        mTextPaint.setTextSize(getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_text_size));

        mInstructionText = new SpannableStringBuilder();
        mInstructionTextLayout = new DynamicLayout(mInstructionText, mInstructionText, mTextPaint,
                mFramingRect.width(), Alignment.ALIGN_CENTER, 1.0f, 0.0f, true);

        mSecurityText = new SpannableStringBuilder();
        mSecurityImg = ContextCompat.getDrawable(context, R.drawable.shield_img);
    }

    public void setInstructionText(String text) {
        mInstructionText.clear();
        mInstructionText.append(text);
    }

    public void setSecurityText(String text) {
        mSecurityText.clear();
        mSecurityText.append(text);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        updateFramingRect();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        drawOverlayMask(canvas);
        drawCorners(canvas);
        drawInstructionText(canvas);
        drawSecurityImg(canvas);
        drawSecurityText(canvas);
    }

    /** Draws transparent scrim around the framing rectangle. */
    private void drawOverlayMask(Canvas canvas) {
        int width = canvas.getWidth();
        int height = canvas.getHeight();

        canvas.drawRect(0, 0, width, mFramingRect.top, mMaskPaint);
        canvas.drawRect(
                0, mFramingRect.top, mFramingRect.left, mFramingRect.bottom + 1, mMaskPaint);
        canvas.drawRect(mFramingRect.right + 1, mFramingRect.top, width, mFramingRect.bottom + 1,
                mMaskPaint);
        canvas.drawRect(0, mFramingRect.bottom + 1, width, height, mMaskPaint);
    }

    /** Draws corners around the framing rectangle. */
    private void drawCorners(Canvas canvas) {
        // Top-left corner
        Path path = new Path();
        path.moveTo(mFramingRect.left, mFramingRect.top + mCornerSize);
        path.lineTo(mFramingRect.left, mFramingRect.top);
        path.lineTo(mFramingRect.left + mCornerSize, mFramingRect.top);
        canvas.drawPath(path, mCornerPaint);

        // Top-right corner
        path.moveTo(mFramingRect.right, mFramingRect.top + mCornerSize);
        path.lineTo(mFramingRect.right, mFramingRect.top);
        path.lineTo(mFramingRect.right - mCornerSize, mFramingRect.top);
        canvas.drawPath(path, mCornerPaint);

        // Bottom-right corner
        path.moveTo(mFramingRect.right, mFramingRect.bottom - mCornerSize);
        path.lineTo(mFramingRect.right, mFramingRect.bottom);
        path.lineTo(mFramingRect.right - mCornerSize, mFramingRect.bottom);
        canvas.drawPath(path, mCornerPaint);

        // Bottom-left corner
        path.moveTo(mFramingRect.left, mFramingRect.bottom - mCornerSize);
        path.lineTo(mFramingRect.left, mFramingRect.bottom);
        path.lineTo(mFramingRect.left + mCornerSize, mFramingRect.bottom);
        canvas.drawPath(path, mCornerPaint);
    }

    /** Draws instruction text below the framing rectangle. */
    private void drawInstructionText(Canvas canvas) {
        canvas.save();
        canvas.translate(mFramingRect.left, mFramingRect.bottom + mInstructionTextTopPadding);
        mInstructionTextLayout.draw(canvas);
        canvas.restore();
    }

    /** Draws security text below the security image. */
    private void drawSecurityText(Canvas canvas) {
        canvas.save();

        // Keep width of security text as 90% of canvas width with alignment center. Remaining 10%
        // should be equally distributed and hence translate the text to begin from 5% on the left.
        // Note that providing 5% (or canvas width / 20) is only required because |translate| method
        // takes both |width| and |height| to translate. We need to translate the text at the
        // required |height| to align it vertically above the framing rectangle.
        int width = canvas.getWidth();
        canvas.translate(Math.round(width / 20.0f), mFramingRect.top / 2);
        mSecurityTextLayout = new DynamicLayout(mSecurityText, mSecurityText, mTextPaint,
                Math.round((width * 9) / 10.0f), Alignment.ALIGN_CENTER, 1.0f, 0.0f, true);
        mSecurityTextLayout.draw(canvas);

        canvas.restore();
    }

    /** Draws the security image. */
    private void drawSecurityImg(Canvas canvas) {
        int width = canvas.getWidth();
        int imageWidth = mSecurityImg.getIntrinsicWidth();
        int imageHeight = mSecurityImg.getIntrinsicHeight();
        // Align the security img horizontally in the center of the canvas. We want to center align
        // both the image and text vertically above the framing rect. We simply keep the security
        // image above the middle of framing rect.
        int securityImageTop = mFramingRect.top / 2 - imageHeight - mSecurityTextTopPadding;
        mSecurityImg.setBounds((width - imageWidth) / 2, securityImageTop,
                width - (width - imageWidth) / 2, securityImageTop + imageHeight);

        mSecurityImg.draw(canvas);
    }

    /** Updates the framing rectangle to always be in the center of the view. */
    private void updateFramingRect() {
        int width = getWidth();
        int height = getHeight();
        mFramingRect = new Rect((width - mRectSize) / 2, (height - mRectSize) / 2,
                mRectSize + (width - mRectSize) / 2, mRectSize + (height - mRectSize) / 2);
    }
}
