// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.text.DynamicLayout;
import android.text.Layout.Alignment;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.view.View;

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
    private final int mTextTopPadding;
    private final DynamicLayout mTextLayout;

    private Rect mFramingRect;
    private SpannableStringBuilder mTextInstructions;

    public AssistantQrCodeCameraPreviewOverlay(Context context) {
        super(context);

        mRectSize = getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_rect_size);
        mCornerSize = getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_corner_size);
        mTextTopPadding = getResources().getDimensionPixelSize(
                R.dimen.guided_browsing_qr_code_overlay_text_top_padding);
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

        mTextInstructions = new SpannableStringBuilder();
        mTextLayout = new DynamicLayout(mTextInstructions, mTextInstructions, mTextPaint,
                mFramingRect.width(), Alignment.ALIGN_CENTER, 1.0f, 0.0f, true);
    }

    public void setTextInstructions(String text) {
        mTextInstructions.clear();
        mTextInstructions.append(text);
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        updateFramingRect();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        drawOverlayMask(canvas);
        drawCorners(canvas);
        drawText(canvas);
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

    /** Draws text below the framing rectangle. */
    private void drawText(Canvas canvas) {
        canvas.save();
        canvas.translate(mFramingRect.left, mFramingRect.bottom + mTextTopPadding);
        mTextLayout.draw(canvas);
        canvas.restore();
    }

    /** Updates the framing rectangle to always be in the center of the view. */
    private void updateFramingRect() {
        int width = getWidth();
        int height = getHeight();
        mFramingRect = new Rect((width - mRectSize) / 2, (height - mRectSize) / 2,
                mRectSize + (width - mRectSize) / 2, mRectSize + (height - mRectSize) / 2);
    }
}
